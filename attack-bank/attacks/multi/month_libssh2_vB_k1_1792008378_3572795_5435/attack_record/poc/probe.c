/* known-hosts entry probe: reads a crafted host line into a known-hosts
   collection and checks a regular host+port against it. */
#include <stdio.h>
#include <string.h>
#include <libssh2.h>

int main(void)
{
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *knh;
    /* bracketed pattern longer than any sane hostname, valid line syntax */
    static const char pattern[] =
        "[aa11:aa11:aa11:aa11:aa11:aa11:aa11:aa11:aa11:aa11:aa11:aa11:"
        "aa11:aa11:aa11:aa11:aa11:aa11:aa11:aa11:aa11:aa11:aa11:aa11:"
        "aa11:aa11:aa11:aa11:aa11:aa11:aa11:aa11:aa11:aa11:aa11:aa11:"
        "aa11:aa11:aa11:aa11:aa11:aa11:aa11:aa11:aa11:aa11:aa11:aa11:"
        "aa11:aa11:aa11:aa11:aa11:aa11:aa11:aa11:aa11:aa11]:22";
    static const char key[] =
        "AAAAB3NzaC1yc2EAAAADAQABAAABgQDTestKey0123456789012345678901234";
    char line[1024];
    int rc;

    if(libssh2_init(0)) {
        fprintf(stderr, "libssh2_init failed\n");
        return 1;
    }

    session = libssh2_session_init();
    knh = session ? libssh2_knownhost_init(session) : NULL;
    if(!knh) {
        fprintf(stderr, "libssh2_knownhost_init failed\n");
        return 1;
    }

    snprintf(line, sizeof(line), "%s ssh-rsa %s", pattern, key);

    rc = libssh2_knownhost_readline(knh, line, strlen(line),
                                    LIBSSH2_KNOWNHOST_FILE_OPENSSH);
    printf("readline: %d\n", rc);

    rc = libssh2_knownhost_checkp(knh, "::2", 22, key, strlen(key),
                                  LIBSSH2_KNOWNHOST_TYPE_PLAIN |
                                  LIBSSH2_KNOWNHOST_KEYENC_BASE64 |
                                  LIBSSH2_KNOWNHOST_KEY_SSHRSA, NULL);
    printf("check: %d\n", rc);

    libssh2_knownhost_free(knh);
    libssh2_session_free(session);
    libssh2_exit();
    return 0;
}
