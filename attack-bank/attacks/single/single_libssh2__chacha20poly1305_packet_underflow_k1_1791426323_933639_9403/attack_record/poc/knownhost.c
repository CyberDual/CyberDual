#include <libssh2.h>

#include <stdio.h>
#include <string.h>

int main(void)
{
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;
    char line[400];
    char *p = line;
    int i;

    if(libssh2_init(0))
        return 1;
    session = libssh2_session_init();
    if(!session)
        return 1;
    hosts = libssh2_knownhost_init(session);
    if(!hosts)
        return 1;

    *p++ = '[';
    for(i = 0; i < 4; i++) {
        memset(p, 'a' + i, 50);
        p += 50;
        *p++ = '.';
    }
    memset(p, 'e', 47);
    p += 47;
    p += sprintf(p, "]:22 ssh-ed25519 "
                 "AAAAC3NzaC1lZDI1NTE5AAAAIB0m5AqVi34rb7f3A1oFfI5vXRLEjkvAD1QxrqbRca4n\n");

    libssh2_knownhost_readline(hosts, line, (size_t)(p - line),
                               LIBSSH2_KNOWNHOST_FILE_OPENSSH);
    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    libssh2_exit();
    return 0;
}
