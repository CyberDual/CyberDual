#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

#include <libssh2.h>

#define SSH_MSG_KEXINIT 20
#define SSH_MSG_KEX_ECDH_INIT 30
#define SSH_MSG_KEX_ECDH_REPLY 31
#define SSH_MSG_NEWKEYS 21
#define SSH_MSG_SERVICE_ACCEPT 6

static void be32(unsigned char *out, uint32_t value)
{
    out[0] = (unsigned char)(value >> 24);
    out[1] = (unsigned char)(value >> 16);
    out[2] = (unsigned char)(value >> 8);
    out[3] = (unsigned char)value;
}

static void put_string(unsigned char **out, const void *data, size_t len)
{
    be32(*out, (uint32_t)len);
    *out += 4;
    if(len) {
        memcpy(*out, data, len);
        *out += len;
    }
}

static int read_exact(int fd, void *data, size_t len)
{
    unsigned char *p = data;
    while(len) {
        ssize_t got = recv(fd, p, len, 0);
        if(got <= 0)
            return -1;
        p += (size_t)got;
        len -= (size_t)got;
    }
    return 0;
}

static int write_exact(int fd, const void *data, size_t len)
{
    const unsigned char *p = data;
    while(len) {
        ssize_t sent = send(fd, p, len, 0);
        if(sent <= 0)
            return -1;
        p += (size_t)sent;
        len -= (size_t)sent;
    }
    return 0;
}

static int read_banner(int fd, unsigned char *banner, size_t *banner_len)
{
    size_t len = 0;
    while(len < 2048) {
        ssize_t got = recv(fd, banner + len, 1, 0);
        if(got != 1)
            return -1;
        len++;
        if(len >= 2 && banner[len - 2] == '\r' && banner[len - 1] == '\n')
            break;
    }
    if(len < 7 || memcmp(banner, "SSH-", 4) != 0)
        return -1;
    *banner_len = len;
    return 0;
}

static int read_packet(int fd, unsigned char **payload, size_t *payload_len)
{
    unsigned char header[4];
    uint32_t packet_len;
    unsigned char *packet;
    unsigned char padding_len;

    if(read_exact(fd, header, 4))
        return -1;
    packet_len = ((uint32_t)header[0] << 24) |
                 ((uint32_t)header[1] << 16) |
                 ((uint32_t)header[2] << 8) |
                 (uint32_t)header[3];
    if(packet_len < 1 || packet_len > 8192)
        return -1;
    packet = malloc(packet_len + 4);
    if(!packet)
        return -1;
    memcpy(packet, header, 4);
    if(read_exact(fd, packet + 4, packet_len)) {
        free(packet);
        return -1;
    }
    padding_len = packet[4];
    if(padding_len < 4 || padding_len >= packet_len) {
        free(packet);
        return -1;
    }
    *payload_len = packet_len - 1 - padding_len;
    *payload = malloc(*payload_len ? *payload_len : 1);
    if(!*payload) {
        free(packet);
        return -1;
    }
    memcpy(*payload, packet + 5, *payload_len);
    free(packet);
    return 0;
}

static int make_packet(unsigned char **packet, size_t *packet_len,
                       const unsigned char *payload, size_t payload_len)
{
    size_t padding_len = 16 - ((5 + payload_len) % 16);
    size_t len = 5 + payload_len + padding_len;

    if(padding_len < 4)
        padding_len += 16;
    len = 5 + payload_len + padding_len;
    *packet = malloc(len);
    if(!*packet)
        return -1;
    be32(*packet, (uint32_t)(len - 4));
    (*packet)[4] = (unsigned char)padding_len;
    memcpy(*packet + 5, payload, payload_len);
    memset(*packet + 5 + payload_len, 0, padding_len);
    *packet_len = len;
    return 0;
}

static int make_kexinit(unsigned char **payload, size_t *payload_len)
{
    static const char * const lists[] = {
        "kex-strict-s-v00@openssh.com,curve25519-sha256",
        "ssh-ed25519",
        "chacha20-poly1305@openssh.com",
        "chacha20-poly1305@openssh.com",
        "hmac-sha2-256",
        "hmac-sha2-256",
        "none",
        "none",
        "",
        "",
    };
    unsigned char cookie[16];
    unsigned char *base;
    unsigned char *out;
    size_t out_len = 1 + 16;
    size_t i;

    if(RAND_bytes(cookie, sizeof(cookie)) != 1)
        return -1;
    for(i = 0; i < 10; i++)
        out_len += 4 + strlen(lists[i]);
    out_len += 5;
    base = calloc(out_len, 1);
    if(!base)
        return -1;
    out = base;
    out[0] = SSH_MSG_KEXINIT;
    memcpy(out + 1, cookie, sizeof(cookie));
    out += 17;
    for(i = 0; i < 10; i++)
        put_string(&out, lists[i], strlen(lists[i]));
    *out++ = 0;
    be32(out, 0);
    *payload = base;
    *payload_len = out_len;
    return 0;
}

static int x25519_keypair(unsigned char public_key[32],
                           unsigned char private_key[32])
{
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, NULL);
    EVP_PKEY *key = NULL;
    size_t len = 32;
    int ok = 0;

    if(!ctx || EVP_PKEY_keygen_init(ctx) <= 0 ||
       EVP_PKEY_keygen(ctx, &key) <= 0)
        goto done;
    if(EVP_PKEY_get_raw_public_key(key, public_key, &len) <= 0 ||
       len != 32)
        goto done;
    len = 32;
    if(EVP_PKEY_get_raw_private_key(key, private_key, &len) <= 0 ||
       len != 32)
        goto done;
    ok = 1;
done:
    EVP_PKEY_free(key);
    EVP_PKEY_CTX_free(ctx);
    return ok ? 0 : -1;
}

static int x25519_shared(unsigned char shared[32],
                         const unsigned char private_key[32],
                         const unsigned char peer_key[32])
{
    EVP_PKEY_CTX *ctx = NULL;
    EVP_PKEY *own = NULL;
    EVP_PKEY *peer = NULL;
    size_t len = 32;
    int ok = 0;

    own = EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, NULL,
                                       private_key, 32);
    peer = EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, NULL,
                                      peer_key, 32);
    if(!own || !peer)
        goto done;
    ctx = EVP_PKEY_CTX_new(own, NULL);
    if(!ctx || EVP_PKEY_derive_init(ctx) <= 0 ||
       EVP_PKEY_derive_set_peer(ctx, peer) <= 0 ||
       EVP_PKEY_derive(ctx, shared, &len) <= 0 || len != 32)
        goto done;
    ok = 1;
done:
    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(own);
    EVP_PKEY_free(peer);
    return ok ? 0 : -1;
}

static int ed25519_keypair(unsigned char public_key[32],
                           unsigned char private_key[32])
{
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL);
    EVP_PKEY *key = NULL;
    size_t len = 32;
    int ok = 0;

    if(!ctx || EVP_PKEY_keygen_init(ctx) <= 0 ||
       EVP_PKEY_keygen(ctx, &key) <= 0)
        goto done;
    if(EVP_PKEY_get_raw_public_key(key, public_key, &len) <= 0 ||
       len != 32)
        goto done;
    len = 32;
    if(EVP_PKEY_get_raw_private_key(key, private_key, &len) <= 0 ||
       len != 32)
        goto done;
    ok = 1;
done:
    EVP_PKEY_free(key);
    EVP_PKEY_CTX_free(ctx);
    return ok ? 0 : -1;
}

static int ed25519_sign(unsigned char **signature, size_t *signature_len,
                        const unsigned char private_key[32],
                        const unsigned char *data, size_t data_len)
{
    EVP_PKEY *key = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, NULL,
                                                 private_key, 32);
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    size_t len = 0;
    int ok = 0;

    if(!key || !ctx || EVP_DigestSignInit(ctx, NULL, NULL, NULL, key) <= 0)
        goto done;
    if(EVP_DigestSign(ctx, NULL, &len, data, data_len) <= 0)
        goto done;
    *signature = malloc(len);
    if(!*signature)
        goto done;
    if(EVP_DigestSign(ctx, *signature, &len, data, data_len) <= 0)
        goto done;
    *signature_len = len;
    ok = 1;
done:
    if(!ok)
        free(*signature);
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(key);
    return ok ? 0 : -1;
}

static int chacha_block(const unsigned char key[32], unsigned char out[64],
                        unsigned int counter)
{
    const EVP_CIPHER *cipher = EVP_chacha20();
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    unsigned char iv[16] = { 0 };
    int out_len = 0;
    int ok = 0;
    int final_len = 0;

    iv[0] = (unsigned char)counter;
    iv[1] = (unsigned char)(counter >> 8);
    iv[2] = (unsigned char)(counter >> 16);
    iv[3] = (unsigned char)(counter >> 24);
    if(!ctx || EVP_EncryptInit_ex(ctx, cipher, NULL, key, iv) <= 0 ||
       EVP_CipherUpdate(ctx, out, &out_len, out, 64) <= 0 || out_len != 64 ||
       EVP_CipherFinal_ex(ctx, out + out_len, &final_len) <= 0 ||
       final_len != 0)
        goto done;
    ok = 1;
done:
    EVP_CIPHER_CTX_free(ctx);
    return ok ? 0 : -1;
}

static int poly1305_tag(const unsigned char key[32], const unsigned char *data,
                        size_t data_len, unsigned char tag[16])
{
    EVP_MAC *mac = NULL;
    EVP_MAC_CTX *ctx = NULL;
    OSSL_PARAM params[2];
    size_t out_len = 16;
    int ok = 0;

    mac = EVP_MAC_fetch(NULL, "POLY1305", NULL);
    if(!mac)
        return -1;
    ctx = EVP_MAC_CTX_new(mac);
    if(!ctx)
        goto done;
    params[0] = OSSL_PARAM_construct_octet_string("key",
                                                  (void *)(uintptr_t)key, 32);
    params[1] = OSSL_PARAM_construct_end();
    if(EVP_MAC_init(ctx, key, 32, params) <= 0 ||
       EVP_MAC_update(ctx, data, data_len) <= 0 ||
       EVP_MAC_final(ctx, tag, &out_len, 16) <= 0 || out_len != 16)
        goto done;
    ok = 1;
done:
    EVP_MAC_CTX_free(ctx);
    EVP_MAC_free(mac);
    return ok ? 0 : -1;
}

static int encrypt_packet(unsigned char **packet, size_t *packet_len,
                          const unsigned char secret[64],
                          const unsigned char *payload, size_t payload_len)
{
    unsigned char poly_key[32] = { 0 };
    unsigned char stream[64] = { 0 };
    unsigned char *body;
    unsigned char *out;
    size_t body_len = 3;

    if(payload_len != 1)
        return -1;
    out = calloc(4 + body_len + 16, 1);
    body = malloc(body_len);
    if(!out || !body) {
        free(out);
        free(body);
        return -1;
    }
    body[0] = 1;
    body[1] = payload[0];
    body[2] = 0;

    if(chacha_block(secret + 32, stream, 0))
        goto fail;
    out[0] = (unsigned char)(body_len >> 24) ^ stream[0];
    out[1] = (unsigned char)(body_len >> 16) ^ stream[1];
    out[2] = (unsigned char)(body_len >> 8) ^ stream[2];
    out[3] = (unsigned char)body_len ^ stream[3];

    memset(stream, 0, sizeof(stream));
    if(chacha_block(secret, stream, 1))
        goto fail;
    {
        size_t i;
        for(i = 0; i < body_len; i++)
            out[4 + i] = body[i] ^ stream[i];
    }

    memset(stream, 0, sizeof(stream));
    if(chacha_block(secret, stream, 0))
        goto fail;
    if(poly1305_tag(stream, out, 4 + body_len, out + 4 + body_len))
        goto fail;

    *packet = out;
    *packet_len = 4 + body_len + 16;
    free(body);
    return 0;
fail:
    free(out);
    free(body);
    return -1;
}

static int hash_update(SHA256_CTX *ctx, const void *data, size_t len)
{
    unsigned char prefix[4];
    be32(prefix, (uint32_t)len);
    return SHA256_Update(ctx, prefix, 4) == 1 &&
           SHA256_Update(ctx, data, len) == 1;
}

static int exchange_hash(unsigned char hash[32],
                         const unsigned char *client_banner,
                         size_t client_banner_len,
                         const unsigned char *server_banner,
                         size_t server_banner_len,
                         const unsigned char *client_kexinit,
                         size_t client_kexinit_len,
                         const unsigned char *server_kexinit,
                         size_t server_kexinit_len,
                         const unsigned char *host_key, size_t host_key_len,
                         const unsigned char client_key[32],
                         const unsigned char server_key[32],
                         const unsigned char shared[32])
{
    SHA256_CTX ctx;
    unsigned char mpint[37];
    size_t shared_len = 32;
    size_t mpint_len;

    while(shared_len && shared[0] == 0)
        shared_len++;
    if(!shared_len || shared[0] & 0x80)
        mpint_len = shared_len + 1;
    else
        mpint_len = shared_len;
    be32(mpint, (uint32_t)mpint_len);
    if(shared[0] & 0x80) {
        mpint[4] = 0;
        memcpy(mpint + 5, shared, shared_len);
    }
    else {
        memcpy(mpint + 4, shared, shared_len);
    }

    if(SHA256_Init(&ctx) != 1)
        return -1;
    if(!hash_update(&ctx, client_banner, client_banner_len - 2) ||
       !hash_update(&ctx, server_banner, server_banner_len - 2) ||
       !hash_update(&ctx, client_kexinit, client_kexinit_len) ||
       !hash_update(&ctx, server_kexinit, server_kexinit_len) ||
       !hash_update(&ctx, host_key, host_key_len) ||
       !hash_update(&ctx, client_key, 32) ||
       !hash_update(&ctx, server_key, 32) ||
       !SHA256_Update(&ctx, mpint, mpint_len + 4))
        return -1;
    return SHA256_Final(hash, &ctx) == 1 ? 0 : -1;
}

static int derive_secret(unsigned char secret[64], const unsigned char shared[32],
                         const unsigned char hash[32])
{
    unsigned char mpint[37];
    unsigned char first[32];
    unsigned char second[32];
    SHA256_CTX ctx;
    size_t shared_len = 32;
    size_t mpint_len;

    while(shared_len && shared[0] == 0)
        shared_len--;
    if(!shared_len || shared[0] & 0x80)
        mpint_len = shared_len + 1;
    else
        mpint_len = shared_len;
    be32(mpint, (uint32_t)mpint_len);
    if(shared[0] & 0x80) {
        mpint[4] = 0;
        memcpy(mpint + 5, shared, shared_len);
    }
    else {
        memcpy(mpint + 4, shared, shared_len);
    }

    if(SHA256_Init(&ctx) != 1 ||
       SHA256_Update(&ctx, mpint, mpint_len + 4) != 1 ||
       SHA256_Update(&ctx, hash, 32) != 1 ||
       SHA256_Update(&ctx, "D", 1) != 1 ||
       SHA256_Update(&ctx, hash, 32) != 1 ||
       SHA256_Final(first, &ctx) != 1)
        return -1;
    if(SHA256_Init(&ctx) != 1 ||
       SHA256_Update(&ctx, mpint, mpint_len + 4) != 1 ||
       SHA256_Update(&ctx, hash, 32) != 1 ||
       SHA256_Update(&ctx, first, 32) != 1 ||
       SHA256_Final(second, &ctx) != 1)
        return -1;
    memcpy(secret, first, 32);
    memcpy(secret + 32, second, 32);
    return 0;
}

static int server_exchange(int fd)
{
    const unsigned char client_version[] = "SSH-2.0-libssh2\r\n";
    const unsigned char server_version[] = "SSH-2.0-benchmark\r\n";
    unsigned char client_banner[2048];
    unsigned char *client_kexinit = NULL;
    unsigned char *server_kexinit = NULL;
    unsigned char *client_kex_packet = NULL;
    unsigned char *server_kex_packet = NULL;
    unsigned char *reply_packet = NULL;
    unsigned char *newkeys_packet = NULL;
    unsigned char *encrypted_packet = NULL;
    unsigned char *host_key = NULL;
    unsigned char *signature = NULL;
    unsigned char *reply = NULL;
    unsigned char client_x25519[32];
    unsigned char server_x25519_private[32];
    unsigned char server_x25519_public[32];
    unsigned char shared[32];
    unsigned char host_public[32];
    unsigned char host_private[32];
    unsigned char hash[32];
    unsigned char secret[64];
    size_t client_banner_len;
    size_t client_kexinit_len;
    size_t server_kexinit_len;
    size_t client_kex_packet_len;
    size_t server_kex_packet_len;
    size_t reply_packet_len;
    size_t newkeys_packet_len;
    size_t encrypted_packet_len;
    size_t host_key_len;
    size_t signature_len;
    size_t reply_len;
    unsigned char newkeys = SSH_MSG_NEWKEYS;
    unsigned char service_accept = SSH_MSG_SERVICE_ACCEPT;
    int result = 1;

    if(write_exact(fd, server_version, sizeof(server_version) - 1))
        return 1;
    if(read_banner(fd, client_banner, &client_banner_len))
        return 1;
    if(read_packet(fd, &client_kexinit, &client_kexinit_len))
        return 1;
    if(client_kexinit_len < 1 || client_kexinit[0] != SSH_MSG_KEXINIT)
        goto done;
    if(make_kexinit(&server_kexinit, &server_kexinit_len) ||
       make_packet(&server_kex_packet, &server_kex_packet_len,
                   server_kexinit, server_kexinit_len) ||
       write_exact(fd, server_kex_packet, server_kex_packet_len))
        goto done;

    if(read_packet(fd, &client_kex_packet, &client_kex_packet_len))
        goto done;
    if(client_kex_packet_len != 37 ||
       client_kex_packet[0] != SSH_MSG_KEX_ECDH_INIT ||
       client_kex_packet[1] != 0 || client_kex_packet[2] != 0 ||
       client_kex_packet[3] != 0 || client_kex_packet[4] != 32)
        goto done;
    memcpy(client_x25519, client_kex_packet + 5, 32);

    if(x25519_keypair(server_x25519_public, server_x25519_private) ||
       x25519_shared(shared, server_x25519_private, client_x25519) ||
       ed25519_keypair(host_public, host_private))
        goto done;

    host_key_len = 4 + 11 + 4 + 32;
    host_key = malloc(host_key_len);
    if(!host_key)
        goto done;
    {
        unsigned char *p = host_key;
        put_string(&p, "ssh-ed25519", 11);
        put_string(&p, host_public, 32);
    }

    if(exchange_hash(hash, client_banner, client_banner_len,
                     server_version, sizeof(server_version) - 1,
                     client_kexinit, client_kexinit_len,
                     server_kexinit, server_kexinit_len,
                     host_key, host_key_len, client_x25519,
                     server_x25519_public, shared))
        goto done;
    if(ed25519_sign(&signature, &signature_len, host_private, hash, 32))
        goto done;

    reply_len = 1 + 4 + host_key_len + 4 + 32 + 4 + 4 + 11 + 4 + signature_len;
    reply = malloc(reply_len);
    if(!reply)
        goto done;
    {
        unsigned char *p = reply;
        *p++ = SSH_MSG_KEX_ECDH_REPLY;
        put_string(&p, host_key, host_key_len);
        put_string(&p, server_x25519_public, 32);
        {
            unsigned char sig_blob[4 + 11 + 4 + 64];
            unsigned char *q = sig_blob;
            put_string(&q, "ssh-ed25519", 11);
            put_string(&q, signature, signature_len);
            put_string(&p, sig_blob, (size_t)(q - sig_blob));
        }
    }
    if(make_packet(&reply_packet, &reply_packet_len, reply, reply_len) ||
       write_exact(fd, reply_packet, reply_packet_len))
        goto done;
    if(read_packet(fd, &client_kex_packet, &client_kex_packet_len) ||
       client_kex_packet_len != 1 ||
       client_kex_packet[0] != SSH_MSG_NEWKEYS)
        goto done;

    if(make_packet(&newkeys_packet, &newkeys_packet_len,
                   &newkeys, 1) ||
       write_exact(fd, newkeys_packet, newkeys_packet_len))
        goto done;
    {
        char request[4096];
        if(recv(fd, request, sizeof(request), 0) <= 0)
            goto done;
    }
    if(derive_secret(secret, shared, hash))
        goto done;
    if(encrypt_packet(&encrypted_packet, &encrypted_packet_len,
                      secret, &service_accept, 1) ||
       write_exact(fd, encrypted_packet, encrypted_packet_len))
        goto done;
    shutdown(fd, SHUT_WR);
    {
        char request[4096];
        while(recv(fd, request, sizeof(request), 0) > 0)
            ;
    }
    result = 0;
done:
    free(client_kexinit);
    free(server_kexinit);
    free(client_kex_packet);
    free(server_kex_packet);
    free(reply_packet);
    free(newkeys_packet);
    free(encrypted_packet);
    free(host_key);
    free(signature);
    free(reply);
    close(fd);
    return result;
}

static int client_exchange(int fd)
{
    LIBSSH2_SESSION *session = NULL;
    static const char kex[] = "curve25519-sha256";
    static const char hostkey[] = "ssh-ed25519";
    static const char crypt[] = "chacha20-poly1305@openssh.com";
    static const char mac[] = "hmac-sha2-256";
    static const char comp[] = "none";
    int rc;

    if(libssh2_init(0))
        return 1;
    session = libssh2_session_init();
    if(!session)
        goto done;
    libssh2_session_set_blocking(session, 1);
    if(libssh2_session_method_pref(session, LIBSSH2_METHOD_KEX, kex) ||
       libssh2_session_method_pref(session, LIBSSH2_METHOD_HOSTKEY, hostkey) ||
       libssh2_session_method_pref(session, LIBSSH2_METHOD_CRYPT_CS, crypt) ||
       libssh2_session_method_pref(session, LIBSSH2_METHOD_CRYPT_SC, crypt) ||
       libssh2_session_method_pref(session, LIBSSH2_METHOD_MAC_CS, mac) ||
       libssh2_session_method_pref(session, LIBSSH2_METHOD_MAC_SC, mac) ||
       libssh2_session_method_pref(session, LIBSSH2_METHOD_COMP_CS, comp) ||
       libssh2_session_method_pref(session, LIBSSH2_METHOD_COMP_SC, comp))
        goto done;
    rc = libssh2_session_handshake(session, fd);
    (void)rc;
done:
    if(session)
        libssh2_session_free(session);
    libssh2_exit();
    return 0;
}

int main(void)
{
    int sockets[2];
    pid_t child;

    if(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets))
        return 2;
    child = fork();
    if(child < 0) {
        close(sockets[0]);
        close(sockets[1]);
        return 2;
    }
    if(child == 0) {
        close(sockets[0]);
        _exit(server_exchange(sockets[1]));
    }
    close(sockets[1]);
    client_exchange(sockets[0]);
    close(sockets[0]);
    waitpid(child, NULL, 0);
    return 0;
}

