/*
 * Probe: drive libssh2's public known-hosts API with a crafted, attacker
 * influenced known-hosts entry.
 *
 *   - parses a known-hosts line whose host part is a very long (but
 *     well-formed) plain host name via libssh2_knownhost_readline()
 *   - then looks up an unrelated host via libssh2_knownhost_check()
 *
 * Everything goes through the public API in <libssh2.h> with in-contract
 * input. On a faulty tree the check aborts under AddressSanitizer while
 * libssh2 compares the stored entry; on a correct tree the program exits
 * normally with status 0.
 */

#include <libssh2.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* make AddressSanitizer terminate the process with SIGABRT on a finding
   even when ASAN_OPTIONS is not set in the environment */
const char *__asan_default_options(void)
{
    return "abort_on_error=1";
}

int main(void)
{
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;
    const int typemask = LIBSSH2_KNOWNHOST_TYPE_PLAIN |
                         LIBSSH2_KNOWNHOST_KEYENC_BASE64 |
                         LIBSSH2_KNOWNHOST_KEY_SSHRSA;
    static const char key[] =
        "AAAAB3NzaC1yc2EAAAADAQABAAABAQC7g1p4m0xYyfF0zT4pZ1lF9cK2"
        "vB8nQ5wR6tY3uI0oP9aS7dF4gH6jK1lZ2xC5vB3nM8qW0eR9tY6uI4o";
    char line[600];
    char hostname[400];
    size_t i;
    int rc;

    /* a long but well-formed host name: dot-separated labels of letters */
    for(i = 0; i < sizeof(hostname) - 1; i++)
        hostname[i] = ((i % 64) == 63) ? '.' : 'a';
    hostname[sizeof(hostname) - 1] = '\0';

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

    /* <host> <key-type> <base64-key> */
    snprintf(line, sizeof(line), "%s ssh-rsa %s", hostname, key);

    /* add the crafted entry; older/correct versions may reject the long
       host name here, which is fine: no entry is stored then */
    rc = libssh2_knownhost_readline(hosts, line, strlen(line),
                                    LIBSSH2_KNOWNHOST_FILE_OPENSSH);
    fprintf(stderr, "readline rc=%d\n", rc);

    /* look up a different host; this compares against the stored entry */
    rc = libssh2_knownhost_check(hosts, "nomatch.example", key, strlen(key),
                                 typemask, NULL);
    fprintf(stderr, "check rc=%d\n", rc);

    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    libssh2_exit();

    return 0;
}
