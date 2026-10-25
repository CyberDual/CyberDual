/*
 * known-hosts parser probe.
 *
 * Adds a bracketed IPv6 known-host entry, then checks host+port through the
 * public API. In-contract input: a regular OpenSSH known_hosts line and a
 * regular libssh2_knownhost_checkp() lookup.
 */
#include <stdio.h>
#include <string.h>

#include <libssh2.h>

int main(void)
{
    static const char line[] =
        "[2001:db8::1] ssh-rsa "
        "AAAAB3NzaC1yc2EAAAADAQABAAABgQDbtn0Y9R0uXqCjw0JL9n\n";
    static const char key[] =
        "AAAAB3NzaC1yc2EAAAADAQABAAABgQDbtn0Y9R0uXqCjw0JL9n";
    const int typemask = LIBSSH2_KNOWNHOST_TYPE_PLAIN |
                         LIBSSH2_KNOWNHOST_KEYENC_BASE64 |
                         LIBSSH2_KNOWNHOST_KEY_SSHRSA;
    struct libssh2_knownhost *stored = NULL;
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;
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

    rc = libssh2_knownhost_readline(hosts, line, strlen(line),
                                    LIBSSH2_KNOWNHOST_FILE_OPENSSH);
    if(rc) {
        fprintf(stderr, "libssh2_knownhost_readline failed: %d\n", rc);
        return 1;
    }

    rc = libssh2_knownhost_checkp(hosts, "2001:db8::1", 22,
                                  key, strlen(key), typemask, &stored);
    printf("knownhost checkp result: %d\n", rc);

    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    libssh2_exit();
    return 0;
}
