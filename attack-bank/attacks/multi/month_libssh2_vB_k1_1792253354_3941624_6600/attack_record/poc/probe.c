#include <libssh2.h>

#include <stdio.h>
#include <string.h>

int main(void)
{
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;
    #define KEY_TEXT "AAAAC3NzaC1lZDI1NTE5AAAAIExample"
    static const char key[] = KEY_TEXT;
    static const char ipv6_line[] = "[0:0:0:0:0:0:0:1]:22 ssh-rsa " KEY_TEXT;
    const int typemask = LIBSSH2_KNOWNHOST_TYPE_PLAIN |
                         LIBSSH2_KNOWNHOST_KEYENC_BASE64 |
                         LIBSSH2_KNOWNHOST_KEY_SSHRSA;
    char line[512];
    size_t line_len;
    int rc;

    if(libssh2_init(0)) {
        fprintf(stderr, "libssh2_init failed\n");
        return 1;
    }

    session = libssh2_session_init();
    if(!session) {
        fprintf(stderr, "libssh2_session_init failed\n");
        libssh2_exit();
        return 1;
    }

    hosts = libssh2_knownhost_init(session);
    if(!hosts) {
        fprintf(stderr, "libssh2_knownhost_init failed\n");
        libssh2_session_free(session);
        libssh2_exit();
        return 1;
    }

    memset(line, 'a', 255);
    line[255] = ' ';
    memcpy(line + 256, "ssh-rsa ", 8);
    memcpy(line + 264, key, sizeof(key) - 1);
    line_len = 264 + sizeof(key) - 1;

    rc = libssh2_knownhost_readline(hosts, line, line_len,
                                    LIBSSH2_KNOWNHOST_FILE_OPENSSH);
    printf("long known-host line returned %d\n", rc);

    rc = libssh2_knownhost_readline(hosts, ipv6_line,
                                    sizeof(ipv6_line) - 1,
                                    LIBSSH2_KNOWNHOST_FILE_OPENSSH);
    if(rc) {
        fprintf(stderr, "IPv6 known-host line failed: %d\n", rc);
        libssh2_knownhost_free(hosts);
        libssh2_session_free(session);
        libssh2_exit();
        return 2;
    }

    rc = libssh2_knownhost_checkp(hosts, "::1", 22, key, sizeof(key) - 1,
                                  typemask, NULL);
    printf("knownhost_checkp returned %d\n", rc);

    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    libssh2_exit();

    return rc == LIBSSH2_KNOWNHOST_CHECK_MATCH ? 0 : 2;
}
