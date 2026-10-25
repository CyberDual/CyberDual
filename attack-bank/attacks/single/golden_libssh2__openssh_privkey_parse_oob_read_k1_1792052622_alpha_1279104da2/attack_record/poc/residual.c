#include <libssh2.h>
#include <stdio.h>
#include <string.h>

/*
 * Unencrypted openssh-key-v1 private key whose private section is only the
 * two checkints and an "ssh-ed25519" type string. No network I/O.
 */
static const char key[] =
    "-----BEGIN OPENSSH PRIVATE KEY-----\n"
    "b3BlbnNzaC1rZXktdjEAAAAABG5vbmUAAAAEbm9uZQAAAAAAAAABAAAAMwAAAAtzc2gtZW\n"
    "QyNTUxOQAAACAREREREREREREREREREREREREREREREREREREREREREQAAABcRIjNEESIz\n"
    "RAAAAAtzc2gtZWQyNTUxOQ==\n"
    "-----END OPENSSH PRIVATE KEY-----\n";

int main(void)
{
    LIBSSH2_SESSION *session;
    int rc;

    if(libssh2_init(0) != 0)
        return 2;

    session = libssh2_session_init();
    if(!session)
        return 3;

    rc = libssh2_userauth_publickey_frommemory(session, "user", 4,
                                               NULL, 0,
                                               key, strlen(key),
                                               "");
    fprintf(stderr, "rc=%d\n", rc);
    libssh2_session_free(session);
    libssh2_exit();
    return rc == 0 ? 0 : 1;
}
