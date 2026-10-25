/* Exercise channel exit-status handling through the public API.
 * A peer may report any uint32 exit status (RFC 4254 §6.10). */

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <libssh2.h>

#define SSH_MSG_SERVICE_REQUEST 5
#define SSH_MSG_SERVICE_ACCEPT 6
#define SSH_MSG_KEXINIT 20
#define SSH_MSG_NEWKEYS 21
#define SSH_MSG_KEX_ECDH_INIT 30
#define SSH_MSG_KEX_ECDH_REPLY 31
#define SSH_MSG_USERAUTH_REQUEST 50
#define SSH_MSG_USERAUTH_SUCCESS 52
#define SSH_MSG_CHANNEL_OPEN 90
#define SSH_MSG_CHANNEL_OPEN_CONFIRMATION 91
#define SSH_MSG_CHANNEL_WINDOW_ADJUST 93
#define SSH_MSG_CHANNEL_EOF 96
#define SSH_MSG_CHANNEL_CLOSE 97
#define SSH_MSG_CHANNEL_REQUEST 98
#define SSH_MSG_CHANNEL_SUCCESS 99

#define MAX_PKT 65536

const char *__asan_default_options(void)
{
    return "abort_on_error=1:halt_on_error=1:detect_leaks=0";
}

static void die(const char *what)
{
    fprintf(stderr, "poc: %s: %s\n", what, strerror(errno));
    exit(1);
}

static void die_msg(const char *what)
{
    fprintf(stderr, "poc: %s\n", what);
    exit(1);
}

static ssize_t read_full(int fd, void *buf, size_t len)
{
    unsigned char *p = buf;
    size_t off = 0;

    while(off < len) {
        ssize_t n = read(fd, p + off, len - off);
        if(n < 0) {
            if(errno == EINTR)
                continue;
            return -1;
        }
        if(n == 0)
            return -1;
        off += (size_t)n;
    }
    return (ssize_t)off;
}

static ssize_t write_full(int fd, const void *buf, size_t len)
{
    const unsigned char *p = buf;
    size_t off = 0;

    while(off < len) {
        ssize_t n = write(fd, p + off, len - off);
        if(n < 0) {
            if(errno == EINTR)
                continue;
            return -1;
        }
        off += (size_t)n;
    }
    return (ssize_t)off;
}

static size_t put_u32(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
    return 4;
}

static uint32_t get_u32(const unsigned char *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static size_t put_mem(unsigned char *p, const void *s, size_t n)
{
    put_u32(p, (uint32_t)n);
    memcpy(p + 4, s, n);
    return 4 + n;
}

static size_t put_cstr(unsigned char *p, const char *s)
{
    return put_mem(p, s, strlen(s));
}

struct sshbuf {
    unsigned char *p;
    size_t n;
    size_t cap;
};

static void buf_init(struct sshbuf *b)
{
    b->cap = 4096;
    b->n = 0;
    b->p = malloc(b->cap);
    if(!b->p)
        die("malloc");
}

static void buf_add(struct sshbuf *b, const void *s, size_t n)
{
    if(b->n + n > b->cap) {
        while(b->n + n > b->cap)
            b->cap *= 2;
        b->p = realloc(b->p, b->cap);
        if(!b->p)
            die("realloc");
    }
    memcpy(b->p + b->n, s, n);
    b->n += n;
}

static void buf_u32(struct sshbuf *b, uint32_t v)
{
    unsigned char t[4];
    put_u32(t, v);
    buf_add(b, t, 4);
}

static void buf_mem(struct sshbuf *b, const void *s, size_t n)
{
    buf_u32(b, (uint32_t)n);
    buf_add(b, s, n);
}

static void buf_cstr(struct sshbuf *b, const char *s)
{
    buf_mem(b, s, strlen(s));
}

struct conn {
    int fd;
    int encrypted;
    uint32_t send_seq;
    uint32_t recv_seq;
    EVP_CIPHER_CTX *enc;
    EVP_CIPHER_CTX *dec;
    unsigned char enc_mac_key[32];
    unsigned char dec_mac_key[32];
};

static void hmac_sha256(const unsigned char *key, size_t key_len,
                        const unsigned char *d1, size_t n1,
                        const unsigned char *d2, size_t n2,
                        unsigned char out[32])
{
    EVP_MAC *mac = EVP_MAC_fetch(NULL, "HMAC", NULL);
    EVP_MAC_CTX *ctx;
    OSSL_PARAM params[2];
    size_t outl = 32;

    if(!mac)
        die_msg("EVP_MAC_fetch");
    ctx = EVP_MAC_CTX_new(mac);
    if(!ctx)
        die_msg("EVP_MAC_CTX_new");
    params[0] = OSSL_PARAM_construct_utf8_string("digest", "SHA256", 0);
    params[1] = OSSL_PARAM_construct_end();
    if(EVP_MAC_init(ctx, key, key_len, params) != 1)
        die_msg("EVP_MAC_init");
    if(EVP_MAC_update(ctx, d1, n1) != 1 || EVP_MAC_update(ctx, d2, n2) != 1)
        die_msg("EVP_MAC_update");
    if(EVP_MAC_final(ctx, out, &outl, 32) != 1)
        die_msg("EVP_MAC_final");
    EVP_MAC_CTX_free(ctx);
    EVP_MAC_free(mac);
}

static void send_packet(struct conn *c, const unsigned char *payload,
                        size_t plen)
{
    size_t block = c->encrypted ? 16 : 8;
    size_t packet_length = plen + 1 + 4;
    size_t pad = block - (packet_length % block);
    unsigned char *pkt;
    size_t total;

    if(pad < 4)
        pad += block;
    total = 4 + 1 + plen + pad;
    pkt = calloc(1, total + (c->encrypted ? 32 : 0));
    if(!pkt)
        die("calloc");
    put_u32(pkt, (uint32_t)(total - 4));
    pkt[4] = (unsigned char)pad;
    memcpy(pkt + 5, payload, plen);
    RAND_bytes(pkt + 5 + plen, pad);

    if(c->encrypted) {
        unsigned char seq[4];
        unsigned char mac[32];
        int outl = 0;

        put_u32(seq, c->send_seq);
        hmac_sha256(c->enc_mac_key, 32, seq, 4, pkt, total, mac);
        if(EVP_EncryptUpdate(c->enc, pkt, &outl, pkt, (int)total) != 1)
            die_msg("encrypt");
        memcpy(pkt + total, mac, 32);
        if(write_full(c->fd, pkt, total + 32) < 0)
            die("write packet");
    }
    else {
        if(write_full(c->fd, pkt, total) < 0)
            die("write packet");
    }
    c->send_seq++;
    free(pkt);
}

static int recv_packet(struct conn *c, unsigned char *payload, size_t *plen)
{
    unsigned char hdr[16];
    unsigned char *pkt = NULL;
    uint32_t pktlen, padding;
    size_t total, payload_len;
    unsigned char seq[4];
    unsigned char mac[32], expect[32];

    if(!c->encrypted) {
        unsigned char lenb[4];
        if(read_full(c->fd, lenb, 4) < 0)
            return -1;
        pktlen = get_u32(lenb);
        if(pktlen < 1 || pktlen > MAX_PKT)
            die_msg("bad plain packet length");
        pkt = malloc(4 + pktlen);
        if(!pkt)
            die("malloc");
        memcpy(pkt, lenb, 4);
        if(read_full(c->fd, pkt + 4, pktlen) < 0)
            die("read plain packet");
        total = 4 + pktlen;
    }
    else {
        int outl = 0;
        size_t have;

        if(read_full(c->fd, hdr, 16) < 0)
            return -1;
        if(EVP_DecryptUpdate(c->dec, hdr, &outl, hdr, 16) != 1)
            die_msg("decrypt hdr");
        pktlen = get_u32(hdr);
        if(pktlen < 12 || pktlen > MAX_PKT || ((4 + pktlen) % 16) != 0)
            die_msg("bad encrypted packet length");
        total = 4 + pktlen;
        pkt = malloc(total);
        if(!pkt)
            die("malloc");
        memcpy(pkt, hdr, 16);
        have = total - 16;
        if(read_full(c->fd, pkt + 16, have) < 0)
            die("read encrypted body");
        if(EVP_DecryptUpdate(c->dec, pkt + 16, &outl, pkt + 16,
                             (int)have) != 1)
            die_msg("decrypt body");
        if(read_full(c->fd, mac, 32) < 0)
            die("read mac");
        put_u32(seq, c->recv_seq);
        hmac_sha256(c->dec_mac_key, 32, seq, 4, pkt, total, expect);
        if(memcmp(mac, expect, 32) != 0)
            die_msg("MAC mismatch");
    }

    padding = pkt[4];
    if(padding + 1 > pktlen)
        die_msg("bad padding");
    payload_len = pktlen - 1 - padding;
    if(payload_len > MAX_PKT)
        die_msg("payload too big");
    memcpy(payload, pkt + 5, payload_len);
    *plen = payload_len;
    c->recv_seq++;
    free(pkt);
    return (int)payload[0];
}

static void sha256(const struct sshbuf *parts, size_t nparts,
                   unsigned char out[32])
{
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    unsigned int outl = 32;
    size_t i;

    if(!ctx || EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1)
        die_msg("sha256 init");
    for(i = 0; i < nparts; i++) {
        if(EVP_DigestUpdate(ctx, parts[i].p, parts[i].n) != 1)
            die_msg("sha256 update");
    }
    if(EVP_DigestFinal_ex(ctx, out, &outl) != 1)
        die_msg("sha256 final");
    EVP_MD_CTX_free(ctx);
}

static void derive(const unsigned char *kmp, size_t kmp_len,
                   const unsigned char h[32], char letter,
                   unsigned char *out, size_t out_len)
{
    unsigned char dig[32];
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    unsigned int outl = 32;

    if(!ctx || EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1)
        die_msg("derive init");
    if(EVP_DigestUpdate(ctx, kmp, kmp_len) != 1 ||
       EVP_DigestUpdate(ctx, h, 32) != 1 ||
       EVP_DigestUpdate(ctx, &letter, 1) != 1 ||
       EVP_DigestUpdate(ctx, h, 32) != 1 ||
       EVP_DigestFinal_ex(ctx, dig, &outl) != 1)
        die_msg("derive");
    EVP_MD_CTX_free(ctx);
    if(out_len > 32)
        die_msg("derive length");
    memcpy(out, dig, out_len);
}

static void mpint_encode(const unsigned char shared[32], struct sshbuf *out)
{
    size_t off = 0;
    size_t blen;
    int high;

    while(off < 32 && shared[off] == 0)
        off++;
    blen = 32 - off;
    if(blen == 0) {
        buf_u32(out, 0);
        return;
    }
    high = (shared[off] & 0x80) != 0;
    buf_u32(out, (uint32_t)(blen + (high ? 1 : 0)));
    if(high) {
        unsigned char z = 0;
        buf_add(out, &z, 1);
    }
    buf_add(out, shared + off, blen);
}

static void server_main(int fd, uint32_t exit_status)
{
    struct conn c;
    unsigned char payload[MAX_PKT];
    size_t plen = 0;
    char banner_line[256];
    size_t blen = 0;
    const char *server_banner = "SSH-2.0-pocserver";
    unsigned char kex_payload[1024];
    size_t kex_len;
    unsigned char client_kex[MAX_PKT];
    size_t client_kex_len = 0;
    EVP_PKEY_CTX *pctx = NULL;
    EVP_PKEY *ours = NULL;
    EVP_PKEY *host = NULL;
    unsigned char our_pub[32], host_pub[32];
    size_t publen;
    unsigned char ks[64];
    size_t ks_len;
    unsigned char H[32];
    unsigned char sig[64];
    size_t siglen = 64;
    unsigned char sigblob[128];
    size_t sigblob_len;
    unsigned char reply[512];
    size_t reply_len;
    int msg;
    uint32_t client_chan = 0;
    int saw_open = 0;
    unsigned char iv_c2s[16], iv_s2c[16], key_c2s[16], key_s2c[16];
    unsigned char mac_c2s[32], mac_s2c[32];
    struct sshbuf kmp;
    char client_banner[256];

    signal(SIGPIPE, SIG_IGN);
    memset(&c, 0, sizeof(c));
    c.fd = fd;

    if(write_full(fd, server_banner, strlen(server_banner)) < 0 ||
       write_full(fd, "\r\n", 2) < 0)
        die("banner write");

    while(blen + 1 < sizeof(banner_line)) {
        char ch;
        if(read_full(fd, &ch, 1) < 0)
            die("banner read");
        if(ch == '\n')
            break;
        if(ch != '\r')
            banner_line[blen++] = ch;
    }
    banner_line[blen] = '\0';
    snprintf(client_banner, sizeof(client_banner), "%s", banner_line);

    /* KEXINIT */
    {
        size_t n = 0;
        kex_payload[n++] = SSH_MSG_KEXINIT;
        RAND_bytes(kex_payload + n, 16);
        n += 16;
        n += put_cstr(kex_payload + n, "curve25519-sha256");
        n += put_cstr(kex_payload + n, "ssh-ed25519");
        n += put_cstr(kex_payload + n, "aes128-ctr");
        n += put_cstr(kex_payload + n, "aes128-ctr");
        n += put_cstr(kex_payload + n, "hmac-sha2-256");
        n += put_cstr(kex_payload + n, "hmac-sha2-256");
        n += put_cstr(kex_payload + n, "none");
        n += put_cstr(kex_payload + n, "none");
        n += put_cstr(kex_payload + n, "");
        n += put_cstr(kex_payload + n, "");
        kex_payload[n++] = 0;
        n += put_u32(kex_payload + n, 0);
        kex_len = n;
    }
    send_packet(&c, kex_payload, kex_len);

    msg = recv_packet(&c, client_kex, &client_kex_len);
    if(msg != SSH_MSG_KEXINIT)
        die_msg("expected KEXINIT");

    msg = recv_packet(&c, payload, &plen);
    if(msg != SSH_MSG_KEX_ECDH_INIT)
        die_msg("expected ECDH_INIT");
    if(plen < 5 || get_u32(payload + 1) != 32 || plen < 5 + 32)
        die_msg("bad client pubkey");

    pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, NULL);
    if(!pctx || EVP_PKEY_keygen_init(pctx) != 1 ||
       EVP_PKEY_keygen(pctx, &ours) != 1)
        die_msg("x25519 keygen");
    publen = 32;
    if(EVP_PKEY_get_raw_public_key(ours, our_pub, &publen) != 1)
        die_msg("x25519 pub");
    EVP_PKEY_CTX_free(pctx);

    pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL);
    if(!pctx || EVP_PKEY_keygen_init(pctx) != 1 ||
       EVP_PKEY_keygen(pctx, &host) != 1)
        die_msg("ed25519 keygen");
    publen = 32;
    if(EVP_PKEY_get_raw_public_key(host, host_pub, &publen) != 1)
        die_msg("ed25519 pub");
    EVP_PKEY_CTX_free(pctx);
    pctx = NULL;

    ks_len = 0;
    ks_len += put_cstr(ks + ks_len, "ssh-ed25519");
    ks_len += put_mem(ks + ks_len, host_pub, 32);

    /* shared secret */
    {
        EVP_PKEY *peer = EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, NULL,
                                                     payload + 5, 32);
        EVP_PKEY_CTX *dctx;
        unsigned char shared[32];
        size_t slen = 32;
        struct sshbuf parts[8];
        size_t np = 0;

        if(!peer)
            die_msg("peer key");
        dctx = EVP_PKEY_CTX_new(ours, NULL);
        if(!dctx || EVP_PKEY_derive_init(dctx) != 1 ||
           EVP_PKEY_derive_set_peer(dctx, peer) != 1 ||
           EVP_PKEY_derive(dctx, shared, &slen) != 1)
            die_msg("x25519 derive");
        EVP_PKEY_CTX_free(dctx);
        EVP_PKEY_free(peer);

        buf_init(&kmp);
        mpint_encode(shared, &kmp);

        buf_init(&parts[np]);
        buf_cstr(&parts[np], client_banner);
        np++;
        buf_init(&parts[np]);
        buf_cstr(&parts[np], server_banner);
        np++;
        buf_init(&parts[np]);
        buf_mem(&parts[np], client_kex, client_kex_len);
        np++;
        buf_init(&parts[np]);
        buf_mem(&parts[np], kex_payload, kex_len);
        np++;
        buf_init(&parts[np]);
        buf_mem(&parts[np], ks, ks_len);
        np++;
        buf_init(&parts[np]);
        buf_mem(&parts[np], payload + 5, 32);
        np++;
        buf_init(&parts[np]);
        buf_mem(&parts[np], our_pub, 32);
        np++;
        parts[np] = kmp;
        np++;
        sha256(parts, np, H);
        for(slen = 0; slen + 1 < np; slen++)
            free(parts[slen].p);
    }

    {
        EVP_MD_CTX *md = EVP_MD_CTX_new();
        if(!md || EVP_DigestSignInit(md, NULL, NULL, NULL, host) != 1 ||
           EVP_DigestSign(md, sig, &siglen, H, 32) != 1)
            die_msg("ed25519 sign");
        EVP_MD_CTX_free(md);
    }

    sigblob_len = 0;
    sigblob_len += put_cstr(sigblob + sigblob_len, "ssh-ed25519");
    sigblob_len += put_mem(sigblob + sigblob_len, sig, siglen);

    reply_len = 0;
    reply[reply_len++] = SSH_MSG_KEX_ECDH_REPLY;
    reply_len += put_mem(reply + reply_len, ks, ks_len);
    reply_len += put_mem(reply + reply_len, our_pub, 32);
    reply_len += put_mem(reply + reply_len, sigblob, sigblob_len);
    send_packet(&c, reply, reply_len);

    payload[0] = SSH_MSG_NEWKEYS;
    send_packet(&c, payload, 1);

    msg = recv_packet(&c, payload, &plen);
    if(msg != SSH_MSG_NEWKEYS)
        die_msg("expected NEWKEYS");

    derive(kmp.p, kmp.n, H, 'A', iv_c2s, 16);
    derive(kmp.p, kmp.n, H, 'B', iv_s2c, 16);
    derive(kmp.p, kmp.n, H, 'C', key_c2s, 16);
    derive(kmp.p, kmp.n, H, 'D', key_s2c, 16);
    derive(kmp.p, kmp.n, H, 'E', mac_c2s, 32);
    derive(kmp.p, kmp.n, H, 'F', mac_s2c, 32);
    free(kmp.p);

    c.dec = EVP_CIPHER_CTX_new();
    c.enc = EVP_CIPHER_CTX_new();
    if(!c.dec || !c.enc)
        die_msg("cipher ctx");
    if(EVP_DecryptInit_ex(c.dec, EVP_aes_128_ctr(), NULL, key_c2s, iv_c2s) != 1)
        die_msg("dec init");
    if(EVP_EncryptInit_ex(c.enc, EVP_aes_128_ctr(), NULL, key_s2c, iv_s2c) != 1)
        die_msg("enc init");
    memcpy(c.dec_mac_key, mac_c2s, 32);
    memcpy(c.enc_mac_key, mac_s2c, 32);
    c.encrypted = 1;

    for(;;) {
        msg = recv_packet(&c, payload, &plen);
        if(msg < 0)
            break;
        if(msg == SSH_MSG_SERVICE_REQUEST) {
            unsigned char acc[64];
            size_t n = 0;
            acc[n++] = SSH_MSG_SERVICE_ACCEPT;
            n += put_cstr(acc + n, "ssh-userauth");
            send_packet(&c, acc, n);
        }
        else if(msg == SSH_MSG_USERAUTH_REQUEST) {
            unsigned char ok = SSH_MSG_USERAUTH_SUCCESS;
            send_packet(&c, &ok, 1);
        }
        else if(msg == SSH_MSG_CHANNEL_OPEN) {
            if(plen < 5)
                die_msg("short channel open");
            {
                uint32_t slen = get_u32(payload + 1);
                const unsigned char *rest;
                if(5 + slen + 8 > plen)
                    die_msg("channel open truncated");
                rest = payload + 5 + slen;
                client_chan = get_u32(rest);
                saw_open = 1;
            }
            {
                unsigned char conf[32];
                size_t n = 0;
                conf[n++] = SSH_MSG_CHANNEL_OPEN_CONFIRMATION;
                n += put_u32(conf + n, client_chan);
                n += put_u32(conf + n, 0);          /* sender channel */
                n += put_u32(conf + n, 2 * 1024 * 1024);
                n += put_u32(conf + n, 32768);
                send_packet(&c, conf, n);
            }
        }
        else if(msg == SSH_MSG_CHANNEL_REQUEST && saw_open) {
            uint32_t slen;
            const char *req;
            unsigned char want = 0;
            if(plen < 9)
                continue;
            slen = get_u32(payload + 5);
            if(9 + slen > plen)
                continue;
            req = (const char *)(payload + 9);
            if(9 + slen < plen)
                want = payload[9 + slen];
            if(slen == 4 && memcmp(req, "exec", 4) == 0) {
                unsigned char pkt[64];
                size_t n;
                if(want) {
                    n = 0;
                    pkt[n++] = SSH_MSG_CHANNEL_SUCCESS;
                    n += put_u32(pkt + n, client_chan);
                    send_packet(&c, pkt, n);
                }
                n = 0;
                pkt[n++] = SSH_MSG_CHANNEL_REQUEST;
                n += put_u32(pkt + n, client_chan);
                n += put_cstr(pkt + n, "exit-status");
                pkt[n++] = 0;
                n += put_u32(pkt + n, exit_status);
                send_packet(&c, pkt, n);
                n = 0;
                pkt[n++] = SSH_MSG_CHANNEL_EOF;
                n += put_u32(pkt + n, client_chan);
                send_packet(&c, pkt, n);
                n = 0;
                pkt[n++] = SSH_MSG_CHANNEL_CLOSE;
                n += put_u32(pkt + n, client_chan);
                send_packet(&c, pkt, n);
            }
            else if(want) {
                unsigned char pkt[8];
                size_t n = 0;
                pkt[n++] = 100; /* CHANNEL_FAILURE */
                n += put_u32(pkt + n, client_chan);
                send_packet(&c, pkt, n);
            }
        }
        else if(msg == SSH_MSG_CHANNEL_CLOSE || msg == SSH_MSG_CHANNEL_EOF) {
            if(msg == SSH_MSG_CHANNEL_EOF && saw_open) {
                unsigned char pkt[8];
                size_t n = 0;
                pkt[n++] = SSH_MSG_CHANNEL_CLOSE;
                n += put_u32(pkt + n, client_chan);
                send_packet(&c, pkt, n);
            }
        }
        else if(msg == SSH_MSG_CHANNEL_WINDOW_ADJUST) {
            /* nothing to do */
        }
    }

    EVP_PKEY_free(ours);
    EVP_PKEY_free(host);
    if(c.enc)
        EVP_CIPHER_CTX_free(c.enc);
    if(c.dec)
        EVP_CIPHER_CTX_free(c.dec);
    close(fd);
    _exit(0);
}

static void show_err(LIBSSH2_SESSION *session, const char *what)
{
    char *msg = NULL;
    int err = libssh2_session_last_error(session, &msg, NULL, 0);
    fprintf(stderr, "client: %s failed (%d): %s\n", what, err,
            msg ? msg : "");
}

static int client_main(int fd, uint32_t exit_status)
{
    LIBSSH2_SESSION *session;
    LIBSSH2_CHANNEL *channel;
    int rc;
    char buf[256];

    (void)exit_status;

    if(libssh2_init(0) != 0)
        die_msg("libssh2_init");
    session = libssh2_session_init();
    if(!session)
        die_msg("session_init");
    libssh2_session_set_blocking(session, 1);
    libssh2_session_method_pref(session, LIBSSH2_METHOD_KEX,
                                "curve25519-sha256");
    libssh2_session_method_pref(session, LIBSSH2_METHOD_HOSTKEY,
                                "ssh-ed25519");
    libssh2_session_method_pref(session, LIBSSH2_METHOD_CRYPT_CS, "aes128-ctr");
    libssh2_session_method_pref(session, LIBSSH2_METHOD_CRYPT_SC, "aes128-ctr");
    libssh2_session_method_pref(session, LIBSSH2_METHOD_MAC_CS, "hmac-sha2-256");
    libssh2_session_method_pref(session, LIBSSH2_METHOD_MAC_SC, "hmac-sha2-256");
    libssh2_session_method_pref(session, LIBSSH2_METHOD_COMP_CS, "none");
    libssh2_session_method_pref(session, LIBSSH2_METHOD_COMP_SC, "none");

    rc = libssh2_session_handshake(session, fd);
    if(rc) {
        show_err(session, "handshake");
        return 1;
    }
    rc = libssh2_userauth_password(session, "user", "pass");
    if(rc) {
        show_err(session, "password");
        return 1;
    }
    channel = libssh2_channel_open_session(session);
    if(!channel) {
        show_err(session, "open_session");
        return 1;
    }
    rc = libssh2_channel_exec(channel, "true");
    if(rc) {
        show_err(session, "exec");
        return 1;
    }
    for(;;) {
        ssize_t n = libssh2_channel_read(channel, buf, sizeof(buf));
        if(n == LIBSSH2_ERROR_EAGAIN)
            continue;
        if(n < 0) {
            show_err(session, "read");
            return 1;
        }
        if(n == 0)
            break;
    }

#ifdef CHECK_HAS
    {
        int has = libssh2_channel_has_exit_status(channel);
        int st = libssh2_channel_get_exit_status(channel);
        LIBSSH2_CHANNEL *other;
        fprintf(stderr, "has=%d status=%d\n", has, st);
        if(exit_status == 0 && (has != 1 || st != 0)) {
            fprintf(stderr, "expected has=1 status=0\n");
            return 1;
        }
        if(!libssh2_channel_has_exit_status(NULL)) {
            /* NULL channel reports 0 */
        }
        else {
            fprintf(stderr, "NULL channel should report 0\n");
            return 1;
        }
        other = libssh2_channel_open_session(session);
        if(!other) {
            show_err(session, "second open");
            return 1;
        }
        /* No command was started, so the peer sends no exit-status.
           A short read timeout keeps the pump from waiting forever. */
        libssh2_session_set_timeout(session, 200);
        libssh2_channel_read(other, buf, sizeof(buf));
        libssh2_session_set_timeout(session, 0);
        if(libssh2_channel_has_exit_status(other) != 0) {
            fprintf(stderr, "channel without a command reported exit-status\n");
            return 1;
        }
        libssh2_channel_close(other);
        libssh2_channel_free(other);
    }
#endif

    libssh2_channel_close(channel);
    libssh2_channel_free(channel);
    libssh2_session_disconnect(session, "done");
    libssh2_session_free(session);
    libssh2_exit();
    close(fd);
    return 0;
}

int main(int argc, char **argv)
{
    int sv[2];
    pid_t pid;
    uint32_t exit_status = 0xffffffffu;
    int status = 0;

    if(argc > 1)
        exit_status = (uint32_t)strtoul(argv[1], NULL, 0);

    signal(SIGPIPE, SIG_IGN);
    if(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0)
        die("socketpair");
    pid = fork();
    if(pid < 0)
        die("fork");
    if(pid == 0) {
        close(sv[0]);
        server_main(sv[1], exit_status);
        _exit(0);
    }
    close(sv[1]);
    if(client_main(sv[0], exit_status) != 0) {
        kill(pid, SIGTERM);
        waitpid(pid, &status, 0);
        return 1;
    }
    close(sv[0]);
    waitpid(pid, &status, 0);
    return 0;
}
