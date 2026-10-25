#include <libssh2.h>

#include <stdio.h>
#include <string.h>

int main(void)
{
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;
    int rc;

    const char line[] =
        "host.example.com aaaaaaaaaaaaaaaaaaaa AAAA";

    if(libssh2_init(0))
        return 2;

    session = libssh2_session_init();
    if(!session)
        return 2;

    hosts = libssh2_knownhost_init(session);
    if(!hosts)
        return 2;

    rc = libssh2_knownhost_readline(hosts, line, sizeof(line) - 1,
                                    LIBSSH2_KNOWNHOST_FILE_OPENSSH);
    fprintf(stderr, "readline rc=%d (no crash)\n", rc);

    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    libssh2_exit();
    return 0;
}
