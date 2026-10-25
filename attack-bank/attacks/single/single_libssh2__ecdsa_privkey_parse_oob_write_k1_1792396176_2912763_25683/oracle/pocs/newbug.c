#include <libssh2.h>

#include <stdio.h>
#include <string.h>

int main(void)
{
    char line[400];
    size_t linelen;
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;
    int rc;

    /* known-hosts line with a long plain hostname */
    memset(line, 'a', 300);
    memcpy(line + 300, " ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQ", 38);
    linelen = 338;

    libssh2_init(0);
    session = libssh2_session_init();
    if(!session)
        return 2;

    hosts = libssh2_knownhost_init(session);
    if(!hosts)
        return 2;

    rc = libssh2_knownhost_readline(hosts, line, linelen,
                                    LIBSSH2_KNOWNHOST_FILE_OPENSSH);
    printf("readline rc=%d\n", rc);

    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    libssh2_exit();
    return 0;
}
