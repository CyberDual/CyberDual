#include <libssh2.h>
#include <stdio.h>

static const char key[] =
    "-----BEGIN OPENSSH PRIVATE KEY-----\n"
    "b3BlbnNzaC1rZXktdjEAAAAACmFlczEyOC1jdHIAAAAGYmNyeXB0AAAAGAAAABAB\n"
    "AgMEBQYHCAkKCwwNDg8QAAAAAQAAAAEAAAABeAAAABAdSqUc3M8ffmRxpKYpCVx5\n"
    "-----END OPENSSH PRIVATE KEY-----\n";

int main(void)
{
    LIBSSH2_SESSION *session;
    int rc;

    if(libssh2_init(0) != 0)
        return 1;
    session = libssh2_session_init();
    if(!session)
        return 1;
    rc = libssh2_userauth_publickey_frommemory(session,
                                               "u", 1,
                                               NULL, 0,
                                               key, sizeof(key) - 1,
                                               "pass");
    printf("result=%d\n", rc);
    libssh2_session_free(session);
    libssh2_exit();
    return 0;
}
