#include <libssh2.h>
#include <stdio.h>
#include <string.h>

/* Unencrypted OpenSSH private key, ECDSA P-256. */
static const char privkey[] =
    "-----BEGIN OPENSSH PRIVATE KEY-----\n"
    "b3BlbnNzaC1rZXktdjEAAAAABG5vbmUAAAAEbm9uZQAAAAAAAAABAAAAaAAAABNlY2RzYS\n"
    "1zaGEyLW5pc3RwMjU2AAAACG5pc3RwMjU2AAAAQQQRERERERERERERERERERERERERERER\n"
    "ERERERERERERERERERERERERERERERERERERERERERERERERERERERERAAAAoBEiM0QRIj\n"
    "NEAAAAE2VjZHNhLXNoYTItbmlzdHAyNTYAAAAIbmlzdHAyNTYAAABBBBERERERERERERER\n"
    "ERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERERER\n"
    "EAAAAhERERERERERERERERERERERERERERERERERERERERERERAAAAAAECAwQFBgc=\n"
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
                                                privkey, strlen(privkey),
                                                NULL);
    fprintf(stderr, "rc=%d\n", rc);
    libssh2_session_free(session);
    libssh2_exit();
    return 0;
}
