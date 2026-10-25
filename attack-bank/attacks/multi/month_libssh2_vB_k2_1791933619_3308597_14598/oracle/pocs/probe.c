#include <libssh2.h>

#include <stdio.h>
#include <string.h>

int main(void)
{
    static const char key[] =
        "AAAAC3NzaC1lZDI1NTE5AAAAIFEg0II3zbc1yWGhRzIc5KwTRlmx10RQP0sS63+1oMPi";
    char name[251];
    char line[400];
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;
    int rc;

    memset(name, 'a', 63);
    name[63] = '.';
    memset(name + 64, 'b', 63);
    name[127] = '.';
    memset(name + 128, 'c', 63);
    name[191] = '.';
    memset(name + 192, 'd', 58);
    name[250] = '\0';

    if(snprintf(line, sizeof(line), "[%s]:22 ssh-ed25519 %s\n", name, key)
       >= (int)sizeof(line))
        return 1;

    if(libssh2_init(0))
        return 2;
    session = libssh2_session_init();
    if(!session)
        return 3;
    hosts = libssh2_knownhost_init(session);
    if(!hosts)
        return 4;

    rc = libssh2_knownhost_readline(hosts, line, strlen(line),
                                    LIBSSH2_KNOWNHOST_FILE_OPENSSH);
    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    libssh2_exit();
    (void)rc;
    return 0;
}
