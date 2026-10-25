#include <libssh2.h>
#include <stdio.h>
#include <string.h>

/* OpenSSH ECDSA private key whose scalar is a canonical zero mpint. */
static const char key[] =
    "-----BEGIN OPENSSH PRIVATE KEY-----\nb3BlbnNzaC1rZXktdjEAAAAA"
    "BG5vbmUAAAAEbm9uZQAAAAAAAAABAAAAAQAAAAB4AQIDBA\nECAwQAAAATZWN"
    "kc2Etc2hhMi1uaXN0cDI1NgAAAAhuaXN0cDI1NgAAAEEEERERERERERER\nER"
    "EREREREREREREREREREREREREREREiIiIiIiIiIiIiIiIiIiIiIiIiIiIiIi"
    "IiIiIiIi\nIiIgAAAAAAAAAA\n-----END OPENSSH PRIVATE KEY-----\n";

int main(void)
{
    LIBSSH2_SESSION *session;
    int rc;

    if(libssh2_init(0) != 0)
        return 1;
    session = libssh2_session_init();
    if(!session)
        return 1;

    rc = libssh2_userauth_publickey_frommemory(session, "user", 4,
                                                NULL, 0,
                                                key, strlen(key),
                                                "");
    printf("rc=%d\n", rc);
    libssh2_session_free(session);
    libssh2_exit();
    return 0;
}
