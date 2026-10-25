#include <libssh2.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    const char line[] =
        "[ ssh-ed25519 "
        "AAAAC3NzaC1lZDI1NTE5AAAAIFA8foTc5+IFCqtcMNCLoh7hrmovve3Y7pRopB8REY3Q";
    size_t line_len = sizeof(line) - 1;
    char *buffer;
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;
    int rc;

    if(libssh2_init(0))
        return 1;

    session = libssh2_session_init();
    if(!session) {
        libssh2_exit();
        return 1;
    }

    hosts = libssh2_knownhost_init(session);
    if(!hosts) {
        libssh2_session_free(session);
        libssh2_exit();
        return 1;
    }

    buffer = malloc(line_len);
    if(!buffer) {
        libssh2_knownhost_free(hosts);
        libssh2_session_free(session);
        libssh2_exit();
        return 1;
    }

    memcpy(buffer, line, line_len);
    rc = libssh2_knownhost_readline(hosts, buffer, line_len,
                                    LIBSSH2_KNOWNHOST_FILE_OPENSSH);

    free(buffer);
    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    libssh2_exit();

    return rc == 0 ? 0 : 1;
}
