/* Drives libssh2_knownhost_readline() on a crafted known-hosts line. */
#include <libssh2.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;
    char line[600];
    int rc;

    libssh2_init(0);

    session = libssh2_session_init();
    if(!session)
        return 2;

    hosts = libssh2_knownhost_init(session);
    if(!hosts)
        return 2;

    /* single hostname token of exactly 256 bytes, then a key */
    memset(line, 'a', 256);
    strcpy(line + 256, " ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQC7");

    rc = libssh2_knownhost_readline(hosts, line, strlen(line),
                                    LIBSSH2_KNOWNHOST_FILE_OPENSSH);
    printf("readline rc=%d\n", rc);

    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    return 0;
}
