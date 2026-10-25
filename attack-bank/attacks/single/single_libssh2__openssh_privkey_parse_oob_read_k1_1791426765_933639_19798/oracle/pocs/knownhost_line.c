#include <libssh2.h>
#include <string.h>

int main(void)
{
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;
    char line[384];
    size_t pos = 0;
    int i;

    if(libssh2_init(0))
        return 1;

    session = libssh2_session_init();
    if(!session)
        return 1;
    hosts = libssh2_knownhost_init(session);
    if(!hosts)
        return 1;

    line[pos++] = '[';
    for(i = 0; i < 4; i++) {
        memset(line + pos, 'a', 62);
        pos += 62;
        if(i != 3)
            line[pos++] = '.';
    }
    memcpy(line + pos,
           "]:22 ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIFEg0II3zbc1yWGh"
           "RzIc5KwTRlmx10RQP0sS63+1oMPi\n",
           sizeof("]:22 ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIFEg0II3zbc1yWGh"
                  "RzIc5KwTRlmx10RQP0sS63+1oMPi\n"));

    libssh2_knownhost_readline(hosts, line, strlen(line),
                               LIBSSH2_KNOWNHOST_FILE_OPENSSH);

    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    libssh2_exit();
    return 0;
}
