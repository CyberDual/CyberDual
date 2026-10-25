/*
 * Probe: drive the libssh2 known-hosts public API with attacker-influenced
 * known-hosts data.
 *
 * No network access is involved: a known-hosts line is parsed directly with
 * libssh2_knownhost_readline() and the collection is then queried with
 * libssh2_knownhost_check(), exactly as an application verifying a server
 * host key would do.
 *
 * On a sound library this completes and prints the check results.
 */

#include <libssh2.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* a realistic RSA host key line (base64 body shortened, content is
   irrelevant to the parser, only its form matters) */
static const char host_key[] =
    "AAAAB3NzaC1yc2EAAAADAQABAAABAQDMKcj0lBTlxEci0uk2GSSObt6w5RU1trrXheNu"
    "8aZ2k2rlXA9SxbJbqpVV7R25y2Nlk1HT";

static void check_host(LIBSSH2_KNOWNHOSTS *hosts, const char *name)
{
    int rc = libssh2_knownhost_check(hosts, name, host_key, strlen(host_key),
                                     LIBSSH2_KNOWNHOST_TYPE_PLAIN |
                                     LIBSSH2_KNOWNHOST_KEYENC_BASE64 |
                                     LIBSSH2_KNOWNHOST_KEY_SSHRSA, NULL);
    printf("check '%s' -> %d\n", name, rc);
}

int main(void)
{
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;
    char line[512];
    int rc;

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

    /* an attacker-influenced known-hosts entry: a compressed IPv6 literal */
    rc = snprintf(line, sizeof(line), "%s %s %s\n",
                  "fd5f:4268:2387:97c1::2", "ssh-rsa", host_key);
    if(rc < 0 || rc >= (int)sizeof(line))
        return 1;
    rc = libssh2_knownhost_readline(hosts, line, strlen(line),
                                    LIBSSH2_KNOWNHOST_FILE_OPENSSH);
    printf("readline(ipv6 entry) -> %d\n", rc);

    /* and a plain hostname entry */
    rc = snprintf(line, sizeof(line), "%s %s %s\n",
                  "node1.example.com", "ssh-rsa", host_key);
    if(rc < 0 || rc >= (int)sizeof(line))
        return 1;
    rc = libssh2_knownhost_readline(hosts, line, strlen(line),
                                    LIBSSH2_KNOWNHOST_FILE_OPENSSH);
    printf("readline(name entry) -> %d\n", rc);

    /* query hosts that are NOT in the file: the library must compare the
       stored names against the queried one (NOTFOUND is expected) */
    check_host(hosts, "fd5f:4268:2387:97c1::3");
    check_host(hosts, "node2.example.com");

    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    libssh2_exit();
    /* skip C-library atexit handlers; the checks above are the point */
    _Exit(0);
}
