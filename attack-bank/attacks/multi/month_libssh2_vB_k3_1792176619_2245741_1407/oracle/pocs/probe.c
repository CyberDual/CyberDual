/*
 * Probe: drive libssh2's public known-hosts API with a crafted
 * known_hosts line (untrusted input: an attacker-influenced host entry).
 *
 * No network access is needed; the parser is driven directly through
 * libssh2_knownhost_readline().
 *
 * On a clean tree this returns 0 after the entry is accepted (the
 * hostname length is in-contract for the library's documented limits).
 * On the defective tree the library aborts under AddressSanitizer while
 * parsing the line.
 */

#include <libssh2.h>

#include <stdio.h>
#include <string.h>

int main(void)
{
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;

    /* a realistic ssh-rsa host key blob (base64); only its textual form
       matters to the known-hosts parser, the key is stored as-is */
    static const char key[] =
        "ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQDdyddj7xJE+wROxQLGvYpD0UXV"
        "2YzDoiy+CVAY6f+S5fmLBCw1y1EJ7/2LFLhFY10aHQfd+mY6rf3ZmLqmeb2wIU9w"
        "TjjhHIcRzE1Xk2m4v2F9zEH7z2cHk8yVQ0h1q2Y5H2yQx0Z5ZqY7Z7VqYh8K2s3N"
        "n0e7BQ1V4Q0vOQ2nH0v+XnTq3hL0s4JtF7c0Z8mF2bB1q0Yd5mN3b8F+FqFh7mPq"
        "uF5dJqY8Zk9qQ2m1Yzq8m0vF2bQ1e6ZQ+0yFqQ9j3mF7L2m8nZ0qF4dQ2y6hQ8m0"
        "zF1qQ9y2mF7L3m8nZ1qF5dQ3y6hQ9m1zF2qQ0y3mF8L4m9nZ2qF6dQ4y7hQ0m2zF"
        "3qQ1y4mF9L5m0nZ3qF7dQ5y8hQ1m3zF4qQ2y5mG0L6m1nZ4qG8dQ6y9hQ2m4";

    /* one single plain hostname of 254 chars followed by the key */
    static char line[1024];
    size_t hostlen = 254;
    int rc;

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

    memset(line, 'a', hostlen);
    line[hostlen] = ' ';
    strcpy(line + hostlen + 1, key);

    rc = libssh2_knownhost_readline(hosts, line, strlen(line),
                                    LIBSSH2_KNOWNHOST_FILE_OPENSSH);
    printf("libssh2_knownhost_readline -> %d\n", rc);

    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    libssh2_exit();
    return 0;
}
