#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libssh2.h>

/* Unencrypted openssh-key-v1 private key. The private section's key-type
   length claims more bytes than the section holds. */
static const char key_pem[] =
    "-----BEGIN OPENSSH PRIVATE KEY-----\n"
    "b3BlbnNzaC1rZXktdjEAAAAABG5vbmUAAAAEbm9uZQAAAAAAAAABAAAAAQEAAAAYAAAAAQ\n"
    "AAAAEAAAAQAEFBQUFBQUFBQUFB\n"
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

    libssh2_session_set_blocking(session, 0);
    rc = libssh2_userauth_publickey_frommemory(session,
                                                "user", 4,
                                                NULL, 0,
                                                key_pem, strlen(key_pem),
                                                "");
    fprintf(stderr, "rc=%d\n", rc);
    libssh2_session_free(session);
    libssh2_exit();
    return 0;
}
