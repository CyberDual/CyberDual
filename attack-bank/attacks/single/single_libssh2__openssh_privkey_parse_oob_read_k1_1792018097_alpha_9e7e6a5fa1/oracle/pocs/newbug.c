#include <stdio.h>

#include "libssh2.h"

static const char key[] =
    "-----BEGIN OPENSSH PRIVATE KEY-----\n"
    "b3BlbnNzaC1rZXktdjEAAAAABG5vbmUAAAAEbm9uZQAAAAAAAAABAAAAAXAAAAAm\n"
    "ESIzRBEiM0QAAAAaeABBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUE=\n"
    "-----END OPENSSH PRIVATE KEY-----\n";

int main(void)
{
    LIBSSH2_SESSION *session;
    int rc;

    if(libssh2_init(0))
        return 1;

    session = libssh2_session_init();
    if(!session)
        return 1;

    rc = libssh2_userauth_publickey_sk(session, "user", 4,
                                       NULL, 0,
                                       key, sizeof(key) - 1,
                                       NULL, NULL, NULL);
    printf("libssh2_userauth_publickey_sk() returned %d\n", rc);

    libssh2_session_free(session);
    libssh2_exit();

    return 0;
}
