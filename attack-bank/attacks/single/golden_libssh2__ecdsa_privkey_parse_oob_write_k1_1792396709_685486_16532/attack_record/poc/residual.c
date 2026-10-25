#include <libssh2.h>

#include <stdio.h>
#include <stdlib.h>

static const char private_key[] =
    "-----BEGIN OPENSSH PRIVATE KEY-----\n"
    "b3BlbnNzaC1rZXktdjEAAAAABG5vbmUAAAAEbm9uZQAAAAAAAAABAAAAaAAAABNlY2RzYS\n"
    "1zaGEyLW5pc3RwMjU2AAAACG5pc3RwMjU2AAAAQQRrF9Hy4SxCR/i85uVjpEDydwN9gS3r\n"
    "M6D0oTlF2JjClk/jQuL+Gn+bjufrSnwPnhYrzjNXazFezsu2QGg3v1H1AAAAmBI0VngSNF\n"
    "Z4AAAAE2VjZHNhLXNoYTItbmlzdHAyNTYAAAAIbmlzdHAyNTYAAABBBGsX0fLhLEJH+Lzm\n"
    "5WOkQPJ3A32BLeszoPShOUXYmMKWT+NC4v4af5uO5+tKfA+eFivOM1drMV7Oy7ZAaDe/Uf\n"
    "UAAAAgAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n"
    "-----END OPENSSH PRIVATE KEY-----\n";

int main(void)
{
    LIBSSH2_SESSION *session;
    int rc;

    if(libssh2_init(0)) {
        fprintf(stderr, "libssh2_init failed\n");
        return EXIT_FAILURE;
    }

    session = libssh2_session_init();
    if(!session) {
        fprintf(stderr, "libssh2_session_init failed\n");
        libssh2_exit();
        return EXIT_FAILURE;
    }

    rc = libssh2_userauth_publickey_frommemory(session, "user", 4,
                                               NULL, 0,
                                               private_key,
                                               sizeof(private_key) - 1,
                                               NULL);
    fprintf(stderr, "libssh2_userauth_publickey_frommemory returned %d\n",
            rc);

    libssh2_session_free(session);
    libssh2_exit();
    return EXIT_SUCCESS;
}
