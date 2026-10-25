#include <libssh2.h>

#include <stdio.h>
#include <string.h>

int main(void)
{
    static const char line[] =
        "0:0:0:0:0:0:0:1 ssh-rsa "
        "AAAAB3NzaC1yc2EAAAADAQABAAABAQ";
    static const char key[] =
        "AAAAB3NzaC1yc2EAAAADAQABAAABAQ";
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;
    int check;

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

    if(libssh2_knownhost_readline(hosts, line, strlen(line),
                                  LIBSSH2_KNOWNHOST_FILE_OPENSSH)) {
        libssh2_knownhost_free(hosts);
        libssh2_session_free(session);
        libssh2_exit();
        return 1;
    }

    check = libssh2_knownhost_check(
        hosts, "::1", key, strlen(key),
        LIBSSH2_KNOWNHOST_TYPE_PLAIN |
        LIBSSH2_KNOWNHOST_KEYENC_BASE64 |
        LIBSSH2_KNOWNHOST_KEY_SSHRSA, NULL);

    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    libssh2_exit();

    printf("knownhost check: %d\n", check);
    return 0;
}
