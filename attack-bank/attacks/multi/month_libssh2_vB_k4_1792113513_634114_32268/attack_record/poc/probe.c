#include <stdio.h>
#include <string.h>

#include <libssh2.h>

int main(void)
{
    char host[256];
    char line[512];
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;

    memset(host, 'a', 255);
    host[255] = '\0';

    if(snprintf(line, sizeof(line),
                "%s ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
                host) >= (int)sizeof(line))
        return 2;

    if(libssh2_init(0))
        return 2;

    session = libssh2_session_init();
    if(!session) {
        libssh2_exit();
        return 2;
    }

    hosts = libssh2_knownhost_init(session);
    if(!hosts) {
        libssh2_session_free(session);
        libssh2_exit();
        return 2;
    }

    (void)libssh2_knownhost_readline(hosts, line, strlen(line),
                                     LIBSSH2_KNOWNHOST_FILE_OPENSSH);

    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    libssh2_exit();
    return 0;
}
