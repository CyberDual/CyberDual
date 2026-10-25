#include <libssh2.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    static const char entry[] =
        "example.com ssh-ed25519 "
        "AAAAC3NzaC1lZDI1NTE5AAAAIO7Dhx/ox7Xoi/sg2GrR7j2x5NxvTGKnMH3wifd6UPCx";
    size_t len = sizeof(entry) - 1;
    char *line = malloc(len);
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;
    int rc;

    if(!line)
        return 2;
    memcpy(line, entry, len);

    if(libssh2_init(0))
        return 2;
    session = libssh2_session_init();
    if(!session)
        return 2;
    hosts = libssh2_knownhost_init(session);
    if(!hosts)
        return 2;

    rc = libssh2_knownhost_readline(hosts, line, len,
                                    LIBSSH2_KNOWNHOST_FILE_OPENSSH);
    printf("readline: %d\n", rc);

    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    libssh2_exit();
    free(line);
    return rc != 0;
}
