/*
 * libssh2 end-to-end driver.
 *
 * Runs a real client session through the public libssh2 API against a
 * minimal SSH server implemented in-process with OpenSSL.  The server
 * completes the version exchange, a curve25519-sha256 key exchange with
 * an ssh-ed25519 host key, password authentication and a "session"
 * channel open, then delivers an ordinary channel exit-signal request
 * and closes the channel.  Every byte on the wire is a well-formed SSH
 * stream; nothing internal to libssh2 is referenced.
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
#include <openssl/hmac.h>
#include <openssl/sha.h>

#include <libssh2.h>

#define POC_BANNER "SSH-2.0-pocsshd_1.0"
#define CIPHER_BLOCK 16
#define MAC_LEN 32
#define MAX_PACKET 32768

static void poc_fail(const char *what)
{
    fprintf(stderr, "poc: %s\n", what);
    exit(2);
}

static void be32(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

static uint32_t rd32(const unsigned char *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static size_t put_string(unsigned char *p, const void *data, size_t len)
{
    be32(p, (uint32_t)len);
    if(len)
        memcpy(p + 4, data, len);
    return len + 4;
}

static void hash_string(EVP_MD_CTX *ctx, const void *p, size_t n)
{
    unsigned char l[4];
    be32(l, (uint32_t)n);
    EVP_DigestUpdate(ctx, l, 4);
    if(n)
        EVP_DigestUpdate(ctx, p, n);
}

static void readn(int fd, void *buf, size_t n)
{
    unsigned char *p = (unsigned char *)buf;
    while(n) {
        ssize_t r = recv(fd, p, n, 0);
        if(r <= 0)
            poc_fail("recv");
        p += r;
        n -= (size_t)r;
    }
}

static void writen(int fd, const void *buf, size_t n)
{
    const unsigned char *p = (const unsigned char *)buf;
    while(n) {
        ssize_t r = send(fd, p, n, 0);
        if(r <= 0)
            poc_fail("send");
        p += r;
        n -= (size_t)r;
    }
}

struct server {
    int fd;
    int encrypted;
    uint32_t seq_tx, seq_rx;
    unsigned char kexinit_c[1024];
    size_t kexinit_c_len;
    unsigned char banner_c[256];
    size_t banner_c_len;
    EVP_PKEY *ed_key;
    unsigned char ed_pub[32];
    EVP_CIPHER_CTX *enc_tx, *dec_rx;
    unsigned char mac_tx[MAC_LEN], mac_rx[MAC_LEN];
};

/* RFC 4253 key derivation from the mpint-encoded shared secret */
static void derive_key(const unsigned char *kval, size_t klen,
                       const unsigned char *h, const unsigned char *sid,
                       size_t sidlen, char letter, unsigned char *out,
                       size_t need)
{
    size_t done = 0;
    while(done < need) {
        unsigned char digest[32];
        unsigned int dlen = sizeof(digest);
        EVP_MD_CTX *ctx = EVP_MD_CTX_new();
        if(!ctx ||
           EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1 ||
           EVP_DigestUpdate(ctx, kval, klen) != 1 ||
           EVP_DigestUpdate(ctx, h, 32) != 1)
            poc_fail("EVP kdf");
        if(done)
            EVP_DigestUpdate(ctx, out, done);
        else {
            EVP_DigestUpdate(ctx, &letter, 1);
            EVP_DigestUpdate(ctx, sid, sidlen);
        }
        if(EVP_DigestFinal_ex(ctx, digest, &dlen) != 1)
            poc_fail("EVP kdf final");
        EVP_MD_CTX_free(ctx);
        memcpy(out + done, digest, need - done >= 32 ? 32 : need - done);
        done += 32;
    }
}

static void send_packet(struct server *s, const unsigned char *payload,
                        size_t len)
{
    unsigned char frame[4096];
    unsigned char cipher[4096];
    unsigned char mac[MAC_LEN];
    unsigned int maclen = MAC_LEN;
    size_t block = s->encrypted ? CIPHER_BLOCK : 8;
    size_t pad, total;

    pad = block - ((5 + len) % block);
    if(pad < 4)
        pad += block;

    total = 4 + 1 + len + pad;
    be32(frame, (uint32_t)(1 + len + pad));
    frame[4] = (unsigned char)pad;
    if(len)
        memcpy(frame + 5, payload, len);
    memset(frame + 5 + len, 0, pad);

    if(s->encrypted) {
        int outl = 0;
        unsigned char seq[4];
        HMAC_CTX *hctx = HMAC_CTX_new();
        be32(seq, s->seq_tx);
        if(!s->enc_tx ||
           EVP_EncryptUpdate(s->enc_tx, cipher, &outl, frame,
                             (int)total) != 1)
            poc_fail("EVP encrypt");
        if(!hctx ||
           HMAC_Init_ex(hctx, s->mac_tx, sizeof(s->mac_tx),
                        EVP_sha256(), NULL) != 1 ||
           HMAC_Update(hctx, seq, 4) != 1 ||
           HMAC_Update(hctx, frame, total) != 1 ||
           HMAC_Final(hctx, mac, &maclen) != 1 || maclen != MAC_LEN)
            poc_fail("HMAC");
        HMAC_CTX_free(hctx);
        writen(s->fd, cipher, total);
        writen(s->fd, mac, maclen);
    }
    else
        writen(s->fd, frame, total);

    s->seq_tx++;
}

static size_t read_packet(struct server *s, unsigned char *payload,
                          size_t max)
{
    unsigned char head[4];
    unsigned char plain[MAX_PACKET + 8];
    unsigned char *wire = NULL;
    uint32_t packet_length;
    unsigned char padding_length;
    size_t payload_len;

    readn(s->fd, head, 4);
    memcpy(plain, head, 4);

    if(!s->encrypted) {
        packet_length = rd32(head);
        if(packet_length < 5 || packet_length > MAX_PACKET)
            poc_fail("plain packet length");
        readn(s->fd, plain + 4, packet_length);
    }
    else {
        unsigned char clear[4];
        unsigned char mac_got[MAC_LEN];
        unsigned char mac_calc[MAC_LEN];
        unsigned char seq[4];
        unsigned int calclen = sizeof(mac_calc);
        HMAC_CTX *hctx;
        int outl = 0;

        if(!s->dec_rx ||
           EVP_DecryptUpdate(s->dec_rx, clear, &outl, head, 4) != 1)
            poc_fail("EVP decrypt head");
        memcpy(plain, clear, 4);
        packet_length = rd32(clear);
        if(packet_length < 16 || packet_length > MAX_PACKET)
            poc_fail("encrypted packet length");

        wire = (unsigned char *)malloc(packet_length + MAC_LEN);
        if(!wire)
            poc_fail("malloc");
        readn(s->fd, wire, packet_length + MAC_LEN);
        if(EVP_DecryptUpdate(s->dec_rx, plain + 4, &outl, wire,
                             (int)packet_length) != 1)
            poc_fail("EVP decrypt body");
        memcpy(mac_got, wire + packet_length, MAC_LEN);

        be32(seq, s->seq_rx);
        hctx = HMAC_CTX_new();
        if(!hctx ||
           HMAC_Init_ex(hctx, s->mac_rx, sizeof(s->mac_rx),
                        EVP_sha256(), NULL) != 1 ||
           HMAC_Update(hctx, seq, 4) != 1 ||
           HMAC_Update(hctx, plain, 4 + packet_length) != 1 ||
           HMAC_Final(hctx, mac_calc, &calclen) != 1 || calclen != MAC_LEN)
            poc_fail("HMAC verify");
        HMAC_CTX_free(hctx);
        if(memcmp(mac_calc, mac_got, MAC_LEN) != 0)
            poc_fail("MAC mismatch");
        free(wire);
    }

    padding_length = plain[4];
    if(padding_length + 1 > packet_length)
        poc_fail("padding");
    payload_len = packet_length - 1 - padding_length;
    if(payload_len == 0 || payload_len > max)
        poc_fail("payload length");
    memcpy(payload, plain + 5, payload_len);
    s->seq_rx++;
    return payload_len;
}

static void read_banner(struct server *s)
{
    size_t len = 0;
    for(;;) {
        char c;
        readn(s->fd, &c, 1);
        if(c == '\n')
            break;
        if(len + 1 < sizeof(s->banner_c))
            s->banner_c[len++] = (unsigned char)c;
        else
            poc_fail("client banner too long");
    }
    while(len && (s->banner_c[len - 1] == '\r'))
        len--;
    s->banner_c_len = len;
}

static void make_ed25519(struct server *s)
{
    static const unsigned char seed[32] = {
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32
    };
    size_t publen = sizeof(s->ed_pub);
    s->ed_key = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, NULL,
                                             seed, sizeof(seed));
    if(!s->ed_key ||
       EVP_PKEY_get_raw_public_key(s->ed_key, s->ed_pub, &publen) != 1 ||
       publen != 32)
        poc_fail("ed25519 key");
}

static void server_session(struct server *s)
{
    static const unsigned char cookie[16] = {
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f
    };
    static const unsigned char x25519_priv[32] = {
        9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 1, 2, 3, 4, 5, 6,
        7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22
    };
    unsigned char payload[2048];
    unsigned char kexinit_s[256];
    unsigned char *p;
    size_t plen, kexinit_s_len;
    unsigned char k_s[128], e[32], x25519_pub[32], shared[32];
    size_t k_s_len, x25519_pub_len = sizeof(x25519_pub), shared_len = 32;
    unsigned char mpint_k[40], h[32], sid[32], sig[64];
    size_t mpint_k_len, sig_len = sizeof(sig);
    unsigned char iv_c2s[16], iv_s2c[16];
    unsigned char key_c2s[16], key_s2c[16];
    unsigned char mac_c2s[MAC_LEN], mac_s2c[MAC_LEN];
    unsigned char kex_reply[512];
    unsigned char frame[64];
    uint32_t client_channel = 0;
    EVP_PKEY *x25519_key = NULL, *peer = NULL;
    EVP_PKEY_CTX *pctx = NULL;
    EVP_MD_CTX *hctx = NULL, *sctx = NULL;
    unsigned int hlen = sizeof(h);

    /* 1: version exchange */
    writen(s->fd, POC_BANNER "\r\n", sizeof(POC_BANNER) + 1);
    read_banner(s);

    /* 2: KEXINIT */
    p = kexinit_s;
    *p++ = 20;
    memcpy(p, cookie, sizeof(cookie));
    p += sizeof(cookie);
    p += put_string(p, "curve25519-sha256", 17);
    p += put_string(p, "ssh-ed25519", 11);
    p += put_string(p, "aes128-ctr", 10);
    p += put_string(p, "aes128-ctr", 10);
    p += put_string(p, "hmac-sha2-256", 13);
    p += put_string(p, "hmac-sha2-256", 13);
    p += put_string(p, "none", 4);
    p += put_string(p, "none", 4);
    p += put_string(p, "", 0);
    p += put_string(p, "", 0);
    *p++ = 0;
    be32(p, 0);
    p += 4;
    kexinit_s_len = (size_t)(p - kexinit_s);

    plen = read_packet(s, payload, sizeof(payload));
    if(plen < 1 || payload[0] != 20)
        poc_fail("expected client KEXINIT");
    memcpy(s->kexinit_c, payload, plen);
    s->kexinit_c_len = plen;

    send_packet(s, kexinit_s, kexinit_s_len);

    /* 3: ECDH init from client */
    plen = read_packet(s, payload, sizeof(payload));
    if(plen != 1 + 4 + 32 || payload[0] != 30 || rd32(payload + 1) != 32)
        poc_fail("expected SSH_MSG_KEX_ECDH_INIT");
    memcpy(e, payload + 5, 32);

    x25519_key = EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, NULL,
                                              x25519_priv,
                                              sizeof(x25519_priv));
    peer = EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, NULL, e, 32);
    pctx = EVP_PKEY_CTX_new(x25519_key, NULL);
    if(!x25519_key || !peer || !pctx ||
       EVP_PKEY_get_raw_public_key(x25519_key, x25519_pub,
                                   &x25519_pub_len) != 1 ||
       EVP_PKEY_derive_init(pctx) != 1 ||
       EVP_PKEY_derive_set_peer(pctx, peer) != 1 ||
       EVP_PKEY_derive(pctx, shared, &shared_len) != 1 || shared_len != 32)
        poc_fail("x25519 kex");

    /* host key blob: string "ssh-ed25519" + string public key */
    p = k_s;
    p += put_string(p, "ssh-ed25519", 11);
    p += put_string(p, s->ed_pub, 32);
    k_s_len = (size_t)(p - k_s);

    /* mpint K */
    {
        const unsigned char *k = shared;
        size_t klen = 32;
        while(klen && k[0] == 0) {
            k++;
            klen--;
        }
        if(!klen)
            poc_fail("zero shared secret");
        if(k[0] & 0x80) {
            be32(mpint_k, (uint32_t)(klen + 1));
            mpint_k[4] = 0;
            memcpy(mpint_k + 5, k, klen);
            mpint_k_len = 5 + klen;
        }
        else {
            be32(mpint_k, (uint32_t)klen);
            memcpy(mpint_k + 4, k, klen);
            mpint_k_len = 4 + klen;
        }
    }

    /* exchange hash */
    hctx = EVP_MD_CTX_new();
    if(!hctx || EVP_DigestInit_ex(hctx, EVP_sha256(), NULL) != 1)
        poc_fail("sha256 init");
    hash_string(hctx, s->banner_c, s->banner_c_len);
    hash_string(hctx, POC_BANNER, sizeof(POC_BANNER) - 1);
    hash_string(hctx, s->kexinit_c, s->kexinit_c_len);
    hash_string(hctx, kexinit_s, kexinit_s_len);
    hash_string(hctx, k_s, k_s_len);
    hash_string(hctx, e, 32);
    hash_string(hctx, x25519_pub, 32);
    EVP_DigestUpdate(hctx, mpint_k, mpint_k_len);
    if(EVP_DigestFinal_ex(hctx, h, &hlen) != 1 || hlen != 32)
        poc_fail("sha256 final");
    memcpy(sid, h, 32);

    /* signature over H */
    sctx = EVP_MD_CTX_new();
    if(!sctx ||
       EVP_DigestSignInit(sctx, NULL, NULL, NULL, s->ed_key) != 1 ||
       EVP_DigestSign(sctx, sig, &sig_len, h, 32) != 1 || sig_len != 64)
        poc_fail("ed25519 sign");

    /* SSH_MSG_KEX_ECDH_REPLY */
    p = kex_reply;
    *p++ = 31;
    p += put_string(p, k_s, k_s_len);
    p += put_string(p, x25519_pub, 32);
    {
        unsigned char sigblob[128];
        unsigned char *q = sigblob;
        q += put_string(q, "ssh-ed25519", 11);
        q += put_string(q, sig, 64);
        p += put_string(p, sigblob, (size_t)(q - sigblob));
    }
    send_packet(s, kex_reply, (size_t)(p - kex_reply));

    /* NEWKEYS, then derive the session keys */
    frame[0] = 21;
    send_packet(s, frame, 1);

    derive_key(mpint_k, mpint_k_len, h, sid, 32, 'A', iv_c2s, 16);
    derive_key(mpint_k, mpint_k_len, h, sid, 32, 'B', iv_s2c, 16);
    derive_key(mpint_k, mpint_k_len, h, sid, 32, 'C', key_c2s, 16);
    derive_key(mpint_k, mpint_k_len, h, sid, 32, 'D', key_s2c, 16);
    derive_key(mpint_k, mpint_k_len, h, sid, 32, 'E', mac_c2s, MAC_LEN);
    derive_key(mpint_k, mpint_k_len, h, sid, 32, 'F', mac_s2c, MAC_LEN);

    s->enc_tx = EVP_CIPHER_CTX_new();
    s->dec_rx = EVP_CIPHER_CTX_new();
    if(!s->enc_tx || !s->dec_rx ||
       EVP_EncryptInit_ex(s->enc_tx, EVP_aes_128_ctr(), NULL,
                          key_s2c, iv_s2c) != 1 ||
       EVP_DecryptInit_ex(s->dec_rx, EVP_aes_128_ctr(), NULL,
                          key_c2s, iv_c2s) != 1)
        poc_fail("cipher init");
    memcpy(s->mac_tx, mac_s2c, MAC_LEN);
    memcpy(s->mac_rx, mac_c2s, MAC_LEN);

    /* client NEWKEYS arrives unencrypted */
    plen = read_packet(s, payload, sizeof(payload));
    if(plen != 1 || payload[0] != 21)
        poc_fail("expected client NEWKEYS");
    s->encrypted = 1;

    /* 4: authenticated service setup */
    for(;;) {
        plen = read_packet(s, payload, sizeof(payload));
        if(plen < 1)
            poc_fail("empty packet");
        if(payload[0] == 5) {              /* SERVICE_REQUEST */
            frame[0] = 6;                  /* SERVICE_ACCEPT */
            put_string(frame + 1, "ssh-userauth", 12);
            send_packet(s, frame, 17);
        }
        else if(payload[0] == 50) {        /* USERAUTH_REQUEST */
            frame[0] = 52;                 /* USERAUTH_SUCCESS */
            send_packet(s, frame, 1);
        }
        else if(payload[0] == 90) {        /* CHANNEL_OPEN */
            size_t typelen;
            if(plen < 1 + 4 || rd32(payload + 1) > 64)
                poc_fail("bad CHANNEL_OPEN");
            typelen = rd32(payload + 1);
            if(plen < 1 + 4 + typelen + 12)
                poc_fail("short CHANNEL_OPEN");
            client_channel = rd32(payload + 5 + typelen);

            p = frame;
            *p++ = 91;                     /* CHANNEL_OPEN_CONFIRMATION */
            be32(p, client_channel); p += 4;
            be32(p, 0); p += 4;            /* our channel number */
            be32(p, 0x200000); p += 4;     /* initial window */
            be32(p, 0x8000); p += 4;       /* max packet size */
            send_packet(s, frame, (size_t)(p - frame));
            break;
        }
        else if(payload[0] == 97 || payload[0] == 98)
            break;
        /* ignore anything else */
    }

    /* 5: a plain channel exit-signal request, then EOF and close */
    if(plen >= 1 && payload[0] == 90) {
        unsigned char req[128];
        unsigned char tail[32];
        p = req;
        *p++ = 98;                         /* SSH_MSG_CHANNEL_REQUEST */
        be32(p, client_channel); p += 4;
        p += put_string(p, "exit-signal", 11);
        *p++ = 0;                          /* want_reply */
        p += put_string(p, "KILL", 4);     /* signal name */
        *p++ = 0;                          /* core dumped */
        p += put_string(p, "", 0);         /* error message */
        p += put_string(p, "", 0);         /* language tag */
        send_packet(s, req, (size_t)(p - req));

        p = tail;
        *p++ = 96;                         /* CHANNEL_EOF */
        be32(p, client_channel); p += 4;
        send_packet(s, tail, (size_t)(p - tail));
        p = tail;
        *p++ = 97;                         /* CHANNEL_CLOSE */
        be32(p, client_channel); p += 4;
        send_packet(s, tail, (size_t)(p - tail));
    }

    /* drain until the client goes away */
    for(;;) {
        unsigned char sink[512];
        ssize_t r = recv(s->fd, sink, sizeof(sink), 0);
        if(r <= 0)
            break;
    }
}

static struct server g_server;

static void *server_thread(void *arg)
{
    (void)arg;
    server_session(&g_server);
    return NULL;
}

int main(void)
{
    int fds[2] = { -1, -1 };
    pthread_t tid;
    LIBSSH2_SESSION *session;
    LIBSSH2_CHANNEL *channel;
    int rc;

    g_server.fd = -1;
    make_ed25519(&g_server);

    if(socketpair(AF_UNIX, SOCK_STREAM, 0, fds))
        poc_fail("socketpair");
    g_server.fd = fds[1];

    if(pthread_create(&tid, NULL, server_thread, NULL))
        poc_fail("pthread_create");

    if(libssh2_init(0))
        poc_fail("libssh2_init");

    session = libssh2_session_init();
    if(!session)
        poc_fail("libssh2_session_init");
    libssh2_session_set_blocking(session, 1);

    /* keep the negotiated algorithms to the one set the server offers */
    if(libssh2_session_method_pref(session, LIBSSH2_METHOD_KEX,
                                   "curve25519-sha256") ||
       libssh2_session_method_pref(session, LIBSSH2_METHOD_HOSTKEY,
                                   "ssh-ed25519") ||
       libssh2_session_method_pref(session, LIBSSH2_METHOD_CRYPT_CS,
                                   "aes128-ctr") ||
       libssh2_session_method_pref(session, LIBSSH2_METHOD_CRYPT_SC,
                                   "aes128-ctr") ||
       libssh2_session_method_pref(session, LIBSSH2_METHOD_MAC_CS,
                                   "hmac-sha2-256") ||
       libssh2_session_method_pref(session, LIBSSH2_METHOD_MAC_SC,
                                   "hmac-sha2-256") ||
       libssh2_session_method_pref(session, LIBSSH2_METHOD_COMP_CS,
                                   "none") ||
       libssh2_session_method_pref(session, LIBSSH2_METHOD_COMP_SC,
                                   "none")) {
        char *msg = NULL;
        libssh2_session_last_error(session, &msg, NULL, 0);
        fprintf(stderr, "poc: could not pin algorithms: %s\n", msg);
        return 2;
    }

    rc = libssh2_session_handshake(session, fds[0]);
    if(rc) {
        char *msg = NULL;
        libssh2_session_last_error(session, &msg, NULL, 0);
        fprintf(stderr, "poc: handshake failed (%d): %s\n", rc, msg);
        return 2;
    }

    if(libssh2_userauth_password(session, "libssh2", "pw")) {
        char *msg = NULL;
        libssh2_session_last_error(session, &msg, NULL, 0);
        fprintf(stderr, "poc: auth failed: %s\n", msg);
        return 2;
    }

    channel = libssh2_channel_open_session(session);
    if(!channel) {
        char *msg = NULL;
        libssh2_session_last_error(session, &msg, NULL, 0);
        fprintf(stderr, "poc: channel open failed: %s\n", msg);
        return 2;
    }

    /* Reading processes everything the server sent for this channel. */
    for(;;) {
        char buf[2048];
        ssize_t n = libssh2_channel_read(channel, buf, sizeof(buf));
        if(n < 0)
            break;
        if(n == 0)
            break;
    }

    {
        char *signame = NULL;
        size_t signame_len = 0;
        if(libssh2_channel_get_exit_signal(channel, &signame,
                                           &signame_len,
                                           NULL, NULL, NULL, NULL))
            poc_fail("get_exit_signal");
        printf("poc: session complete, exit signal: %s\n",
               signame ? signame : "(none)");
        free(signame);
    }

    libssh2_channel_close(channel);
    libssh2_channel_free(channel);
    libssh2_session_disconnect(session, "done");
    libssh2_session_free(session);
    libssh2_exit();

    close(fds[0]);
    pthread_join(tid, NULL);
    close(fds[1]);
    return 0;
}
