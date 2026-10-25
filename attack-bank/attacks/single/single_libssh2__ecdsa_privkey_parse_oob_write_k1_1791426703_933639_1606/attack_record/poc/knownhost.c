#include <libssh2.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    static const char key[] =
        "AAAAC3NzaC1lZDI1NTE5AAAAIFEg0II3zbc1yWGhRzIc5KwTRlmx10RQP0sS63+1oMPi";
    char salt[33];
    char line[256];
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;

    memset(salt, 'A', sizeof(salt) - 1);
    salt[sizeof(salt) - 1] = '\0';
    snprintf(line, sizeof(line), "|1|%s|AAAAAAAAAAAAAAAAAAAAAAAAAAA= "
             "ssh-ed25519 %s", salt, key);

    if(libssh2_init(0))
        return 1;
    session = libssh2_session_init();
    if(!session)
        return 1;
    hosts = libssh2_knownhost_init(session);
    if(!hosts)
        return 1;

    libssh2_knownhost_readline(hosts, line, strlen(line),
                              LIBSSH2_KNOWNHOST_FILE_OPENSSH);

    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    libssh2_exit();
    return 0;
}
