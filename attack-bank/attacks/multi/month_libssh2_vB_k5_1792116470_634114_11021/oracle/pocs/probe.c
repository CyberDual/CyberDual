#include <libssh2.h>

#include <stdio.h>
#include <string.h>

/*
 * Load one OpenSSH known_hosts line and look the host up. The line is a
 * normal public-API input: hostname, key type, base64 key.
 */
int main(void)
{
    static const char line[] =
        "2001:db8: ssh-ed25519 "
        "AAAAC3NzaC1lZDI1NTE5AAAAIOMqqnkVzrm0SdG6UOoqKLsabgH5C9okWi0dh2l9GKJl\n";
    static const char key[] =
        "AAAAC3NzaC1lZDI1NTE5AAAAIOMqqnkVzrm0SdG6UOoqKLsabgH5C9okWi0dh2l9GKJl";
    const int typemask = LIBSSH2_KNOWNHOST_TYPE_PLAIN |
                         LIBSSH2_KNOWNHOST_KEYENC_BASE64 |
                         LIBSSH2_KNOWNHOST_KEY_ED25519;
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;
    int rc;

    if(libssh2_init(0))
        return 1;

    session = libssh2_session_init();
    if(!session)
        return 1;

    hosts = libssh2_knownhost_init(session);
    if(!hosts)
        return 1;

    rc = libssh2_knownhost_readline(hosts, line, strlen(line),
                                    LIBSSH2_KNOWNHOST_FILE_OPENSSH);
    if(rc) {
        fprintf(stderr, "readline failed: %d\n", rc);
        return 2;
    }

    /* Different textual form, so the literal is parsed rather than memcmp'd. */
    rc = libssh2_knownhost_check(hosts, "2001:db8::1", key, strlen(key),
                                 typemask, NULL);
    fprintf(stderr, "check=%d\n", rc);

    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    libssh2_exit();
    return 0;
}
