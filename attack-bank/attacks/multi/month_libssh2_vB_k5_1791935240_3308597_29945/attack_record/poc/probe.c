#include <libssh2.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    static const char key[] = "AAAAC3NzaC1lZDI1NTE5AAAAIGR1bW15";
    char line[512];
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;
    int rc;

    memset(line, 'a', 300);
    snprintf(line + 300, sizeof(line) - 300, " ssh-ed25519 %s\n", key);

    if(libssh2_init(0))
        return 1;
    session = libssh2_session_init();
    if(!session)
        return 2;
    hosts = libssh2_knownhost_init(session);
    if(!hosts)
        return 3;

    rc = libssh2_knownhost_readline(hosts, line, strlen(line),
                                    LIBSSH2_KNOWNHOST_FILE_OPENSSH);
    if(rc) {
        libssh2_knownhost_free(hosts);
        libssh2_session_free(session);
        libssh2_exit();
        return 0;
    }
    rc = libssh2_knownhost_check(hosts, "::1", key, strlen(key),
                                 LIBSSH2_KNOWNHOST_TYPE_PLAIN |
                                 LIBSSH2_KNOWNHOST_KEYENC_BASE64 |
                                 LIBSSH2_KNOWNHOST_KEY_ED25519, NULL);

    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    libssh2_exit();
    return rc == LIBSSH2_KNOWNHOST_CHECK_NOTFOUND ? 0 : 5;
}
