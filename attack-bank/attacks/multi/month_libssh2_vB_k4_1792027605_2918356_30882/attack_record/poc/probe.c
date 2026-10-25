/* libssh2 known-hosts matcher probe.
 *
 * Reads one crafted known-hosts entry and checks a host against it
 * through the public API. Uses only <libssh2.h>.
 */
#include <stdio.h>
#include <string.h>

#include <libssh2.h>

static const char *line =
    "[1:2:3:4:5:6:7:8.8.8.8]:22 ssh-ed25519 "
    "AAAAC3NzaC1lZDI1NTE5AAAAIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";

static const char *key =
    "AAAAC3NzaC1lZDI1NTE5AAAAIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";

int main(void)
{
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *knh;
    struct libssh2_knownhost *stored = NULL;
    int rc;

    if(libssh2_init(0)) {
        fprintf(stderr, "libssh2_init failed\n");
        return 1;
    }

    session = libssh2_session_init();
    if(!session) {
        fprintf(stderr, "session init failed\n");
        return 1;
    }

    knh = libssh2_knownhost_init(session);
    if(!knh) {
        fprintf(stderr, "knownhost init failed\n");
        return 1;
    }

    rc = libssh2_knownhost_readline(knh, line, strlen(line),
                                    LIBSSH2_KNOWNHOST_FILE_OPENSSH);
    if(rc) {
        fprintf(stderr, "readline failed: %d\n", rc);
        return 2;
    }

    rc = libssh2_knownhost_checkp(knh, "::9", 22, key, strlen(key),
                                  LIBSSH2_KNOWNHOST_TYPE_PLAIN |
                                  LIBSSH2_KNOWNHOST_KEYENC_BASE64 |
                                  LIBSSH2_KNOWNHOST_KEY_ED25519,
                                  &stored);
    printf("check result: %d (%s)\n", rc,
           rc == LIBSSH2_KNOWNHOST_CHECK_MATCH ? "match" :
           rc == LIBSSH2_KNOWNHOST_CHECK_MISMATCH ? "mismatch" :
           rc == LIBSSH2_KNOWNHOST_CHECK_NOTFOUND ? "notfound" :
           "failure");

    libssh2_knownhost_free(knh);
    libssh2_session_free(session);
    libssh2_exit();
    return 0;
}
