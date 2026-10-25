#include <libssh2.h>

#include <string.h>

int main(void)
{
    static const char line[] =
        "|1|AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA|AAAAAAAAAAAAAAAAAAAAAAAAAAA= "
        "ssh-ed25519 "
        "AAAAC3NzaC1lZDI1NTE5AAAAIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;

    if(libssh2_init(0))
        return 1;
    session = libssh2_session_init();
    if(!session)
        return 2;
    hosts = libssh2_knownhost_init(session);
    if(!hosts)
        return 3;

    libssh2_knownhost_readline(hosts, line, strlen(line),
                              LIBSSH2_KNOWNHOST_FILE_OPENSSH);

    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    libssh2_exit();
    return 0;
}
