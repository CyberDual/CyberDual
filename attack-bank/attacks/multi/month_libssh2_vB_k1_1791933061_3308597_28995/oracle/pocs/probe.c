#include <libssh2.h>

#include <stdio.h>
#include <string.h>

int main(void)
{
    static const char key[] =
        "AAAAC3NzaC1lZDI1NTE5AAAAIO7Dhx/ox7Xoi/sg2GrR7j2x5NxvTGKnMH3wifd6UPCx";
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;
    char hostname[251];
    char line[400];
    size_t i;

    for(i = 0; i < 250; i++)
        hostname[i] = 'a';
    hostname[60] = '.';
    hostname[121] = '.';
    hostname[182] = '.';
    hostname[243] = '.';
    hostname[250] = '\0';

    if(snprintf(line, sizeof(line), "[%s]:22 ssh-ed25519 %s\n",
                hostname, key) >= (int)sizeof(line))
        return 1;

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
