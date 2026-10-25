#include <libssh2.h>

#include <stdio.h>

int main(void)
{
    static const char private_key[] =
        "-----BEGIN OPENSSH PRIVATE KEY-----\n"
        "b3BlbnNzaC1rZXktdjEAAAAABG5vbmUAAAAEbm9uZQAAAAAAAAABAAAAAXgAAAATTElCVUxJQlUAAAAHc3NoLXJzYQ==\n"
        "-----END OPENSSH PRIVATE KEY-----\n";
    LIBSSH2_SESSION *session;
    int rc;

    if(libssh2_init(0)) {
        fprintf(stderr, "libssh2_init failed\n");
        return 2;
    }

    session = libssh2_session_init();
    if(!session) {
        fprintf(stderr, "libssh2_session_init failed\n");
        libssh2_exit();
        return 2;
    }

    rc = libssh2_userauth_publickey_frommemory(session, "user", 4,
                                                NULL, 0,
                                                private_key,
                                                sizeof(private_key) - 1,
                                                NULL);
    fprintf(stderr, "publickey result: %d\n", rc);

    libssh2_session_free(session);
    libssh2_exit();
    return 0;
}
