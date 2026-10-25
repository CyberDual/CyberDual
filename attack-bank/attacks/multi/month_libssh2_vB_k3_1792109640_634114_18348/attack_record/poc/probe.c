#include <libssh2.h>
#include <string.h>

/*
 * Parse one OpenSSH known_hosts line. The hostname is an IPv6 literal with
 * an RFC 4007 zone identifier, which the known-host parser accepts.
 */
int main(void)
{
    static const char line[] =
        "fe80::1%eth0 ssh-ed25519 "
        "AAAAC3NzaC1lZDI1NTE5AAAAIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n";
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;
    int rc;

    if(libssh2_init(0) != 0)
        return 0;

    session = libssh2_session_init();
    if(!session) {
        libssh2_exit();
        return 0;
    }

    hosts = libssh2_knownhost_init(session);
    if(!hosts) {
        libssh2_session_free(session);
        libssh2_exit();
        return 0;
    }

    rc = libssh2_knownhost_readline(hosts, line, strlen(line),
                                    LIBSSH2_KNOWNHOST_FILE_OPENSSH);
    (void)rc;

    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    libssh2_exit();
    return 0;
}
