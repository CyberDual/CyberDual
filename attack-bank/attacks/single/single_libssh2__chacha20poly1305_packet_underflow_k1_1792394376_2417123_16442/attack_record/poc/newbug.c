/* PoC: drive the known-hosts parser with a hashed (|1|salt|hash) entry.
 *
 * Uses only the public API: libssh2_session_init, libssh2_knownhost_init,
 * libssh2_knownhost_readline.
 */
#include <libssh2.h>

#include <stdio.h>
#include <string.h>

int main(void)
{
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;
    int rc;

    /* a hashed known_hosts line: |1|<base64 salt>|<base64 hash> <type> <key> */
    static const char line[] =
        "|1|AAAAAAAAAAAAAAAAAAAAAAAAAAAA|BBBBBBBBBBBBBBBBBBBBBBBBBBBB "
        "ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQC7vbqajDwA+KBkkN7o7k1l";

    rc = libssh2_init(0);
    if(rc) {
        fprintf(stderr, "libssh2_init failed: %d\n", rc);
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
    printf("libssh2_knownhost_readline returned %d\n", rc);

    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    libssh2_exit();
    return 0;
}
