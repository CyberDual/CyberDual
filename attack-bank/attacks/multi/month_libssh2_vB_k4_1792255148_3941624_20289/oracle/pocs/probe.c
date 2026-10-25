#include <libssh2.h>

#include <stdlib.h>
#include <string.h>

int main(void)
{
    static const char known_hosts_line[] =
        "[ ssh-ed25519 AAAAB3NzaC1yc2EAAAADAQABAAABAQ";
    size_t line_len = sizeof(known_hosts_line) - 1;
    char *line;
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *known_hosts;
    int rc;

    line = malloc(line_len);
    if(!line)
        return 1;

    memcpy(line, known_hosts_line, line_len);

    if(libssh2_init(0))
        return 1;

    session = libssh2_session_init();
    if(!session) {
        free(line);
        libssh2_exit();
        return 1;
    }

    known_hosts = libssh2_knownhost_init(session);
    if(!known_hosts) {
        libssh2_session_free(session);
        free(line);
        libssh2_exit();
        return 1;
    }

    rc = libssh2_knownhost_readline(known_hosts, line, line_len,
                                    LIBSSH2_KNOWNHOST_FILE_OPENSSH);

    libssh2_knownhost_free(known_hosts);
    libssh2_session_free(session);
    free(line);
    libssh2_exit();

    return rc == LIBSSH2_ERROR_NONE ? 0 : 1;
}
