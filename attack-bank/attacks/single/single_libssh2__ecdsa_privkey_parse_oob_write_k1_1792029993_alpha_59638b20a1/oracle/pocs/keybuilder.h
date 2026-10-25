/* Minimal builder for unencrypted openssh-key-v1 (OpenSSH private key)
 * blobs, used by the PoC programs in this directory.
 */
#ifndef KEYBUILDER_H
#define KEYBUILDER_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct kbuf {
    unsigned char *data;
    size_t len;
    size_t cap;
};

static void kb_reserve(struct kbuf *b, size_t extra)
{
    unsigned char *p;
    while(b->len + extra > b->cap)
        b->cap = b->cap ? b->cap * 2 : 256;
    p = realloc(b->data, b->cap);
    if(!p) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    b->data = p;
}

static void kb_raw(struct kbuf *b, const void *p, size_t n)
{
    kb_reserve(b, n);
    if(n) {
        memcpy(b->data + b->len, p, n);
        b->len += n;
    }
}

static void kb_u32(struct kbuf *b, uint32_t v)
{
    unsigned char t[4];
    t[0] = (unsigned char)(v >> 24);
    t[1] = (unsigned char)(v >> 16);
    t[2] = (unsigned char)(v >> 8);
    t[3] = (unsigned char)v;
    kb_raw(b, t, 4);
}

/* ssh "string": uint32 length prefix + raw bytes */
static void kb_str(struct kbuf *b, const void *p, size_t n)
{
    kb_u32(b, (uint32_t)n);
    kb_raw(b, p, n);
}

static void kb_cstr(struct kbuf *b, const char *s)
{
    kb_str(b, s, strlen(s));
}

static const char kb_b64tab[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void kb_base64(struct kbuf *out, const unsigned char *in, size_t n)
{
    size_t i;
    for(i = 0; i + 2 < n; i += 3) {
        unsigned v = ((unsigned)in[i] << 16) |
                     ((unsigned)in[i + 1] << 8) | (unsigned)in[i + 2];
        char t[4];
        t[0] = kb_b64tab[(v >> 18) & 0x3f];
        t[1] = kb_b64tab[(v >> 12) & 0x3f];
        t[2] = kb_b64tab[(v >> 6) & 0x3f];
        t[3] = kb_b64tab[v & 0x3f];
        kb_raw(out, t, 4);
    }
    if(n - i == 1) {
        unsigned v = (unsigned)in[i] << 16;
        char t[4];
        t[0] = kb_b64tab[(v >> 18) & 0x3f];
        t[1] = kb_b64tab[(v >> 12) & 0x3f];
        t[2] = '=';
        t[3] = '=';
        kb_raw(out, t, 4);
    }
    else if(n - i == 2) {
        unsigned v = ((unsigned)in[i] << 16) | ((unsigned)in[i + 1] << 8);
        char t[4];
        t[0] = kb_b64tab[(v >> 18) & 0x3f];
        t[1] = kb_b64tab[(v >> 12) & 0x3f];
        t[2] = kb_b64tab[(v >> 6) & 0x3f];
        t[3] = '=';
        kb_raw(out, t, 4);
    }
}

/* Wrap a raw binary key in a PEM document, 64-character base64 lines. */
static void kb_pem(struct kbuf *out, const unsigned char *in, size_t n)
{
    static const char header[] = "-----BEGIN OPENSSH PRIVATE KEY-----\n";
    static const char footer[] = "-----END OPENSSH PRIVATE KEY-----\n";
    struct kbuf b64buf = { NULL, 0, 0 };
    size_t off;

    kb_raw(out, header, sizeof(header) - 1);

    kb_base64(&b64buf, in, n);
    for(off = 0; off < b64buf.len; off += 64) {
        size_t chunk = b64buf.len - off;
        if(chunk > 64)
            chunk = 64;
        kb_raw(out, b64buf.data + off, chunk);
        kb_raw(out, "\n", 1);
    }
    free(b64buf.data);

    kb_raw(out, footer, sizeof(footer) - 1);
}

/*
 * Assemble an unencrypted openssh-key-v1 ECDSA (nistp256) private key.
 *
 * pub_point_len/pub_point:   point stored in the embedded public key blob
 * point_len/point:           point stored in the private key section
 * exponent_len/exponent:     private exponent mpint
 */
static void kb_build_ecdsa_key(struct kbuf *pem,
                               const unsigned char *pub_point,
                               size_t pub_point_len,
                               const unsigned char *point, size_t point_len,
                               const unsigned char *exponent,
                               size_t exponent_len)
{
    struct kbuf pub = { NULL, 0, 0 };
    struct kbuf priv = { NULL, 0, 0 };
    struct kbuf key = { NULL, 0, 0 };
    uint32_t check = 0x5f13a7c1U;

    /* private key section */
    kb_u32(&priv, check);              /* check 1 */
    kb_u32(&priv, check);              /* check 2 */
    kb_cstr(&priv, "ecdsa-sha2-nistp256");
    kb_cstr(&priv, "nistp256");
    kb_str(&priv, point, point_len);   /* public key point */
    kb_str(&priv, exponent, exponent_len);  /* private exponent */
    kb_cstr(&priv, "libssh2");         /* comment */

    /* embedded public key blob (not validated by libssh2) */
    kb_cstr(&pub, "ecdsa-sha2-nistp256");
    kb_cstr(&pub, "nistp256");
    kb_str(&pub, pub_point, pub_point_len);

    /* openssh-key-v1 envelope */
    kb_raw(&key, "openssh-key-v1", 14);
    kb_raw(&key, "\0", 1);
    kb_cstr(&key, "none");             /* ciphername */
    kb_cstr(&key, "none");             /* kdfname */
    kb_str(&key, NULL, 0);             /* kdf options */
    kb_u32(&key, 1);                   /* number of keys */
    kb_str(&key, pub.data, pub.len);   /* public key */
    kb_str(&key, priv.data, priv.len); /* private key */

    kb_pem(pem, key.data, key.len);

    free(pub.data);
    free(priv.data);
    free(key.data);
}

#endif /* KEYBUILDER_H */
