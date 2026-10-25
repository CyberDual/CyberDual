/* Drives the libssh2 known-hosts line parser (public API) with a crafted
 * OpenSSH hashed host entry.  No network is involved: the parser is fed
 * directly through libssh2_knownhost_readline().
 *
 * The host field of an OpenSSH hashed entry has the form |1|<salt>|<hash>.
 * This input uses a <salt> field of exactly 32 bytes.
 */
#include <libssh2.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;
    /* 32-byte salt field, then a hash field, then a key. */
    static const char line[] =
        "|1|AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA|QUFBQQ== "
        "ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQConeexample";

    if(libssh2_init(0))
        return 2;

    session = libssh2_session_init();
    if(!session)
        return 2;

    hosts = libssh2_knownhost_init(session);
    if(!hosts)
        return 2;

    libssh2_knownhost_readline(hosts, line, strlen(line),
                               LIBSSH2_KNOWNHOST_FILE_OPENSSH);

    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    libssh2_exit();

    printf("parsed without abort\n");
    return 0;
}
