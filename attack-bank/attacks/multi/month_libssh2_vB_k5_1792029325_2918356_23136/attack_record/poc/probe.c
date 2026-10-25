/*
 * known-hosts probe: parse a bracketed IPv6 known-hosts entry and check an
 * equivalent literal against it through the public API.
 */

#include <stdio.h>
#include <string.h>

#include <libssh2.h>

/* An IPv6 literal for the default port, as OpenSSH stores it, followed by a
   regular RSA host key. */
static const char key[] =
    "AAAAB3NzaC1yc2EAAAADAQABAAABgQDCuJnhADGkgnGhDfQAMHBbrGwDzn9tUJ5FytZM0FW"
    "QnxEVAnYA2dBdGQVClN0IKE8j8tEBEBGZUeybB6IABHyvx+cU3rXTxdZLXgPQTFuYlWxhf7"
    "hBLPmOGId5VZ2fXEC2f/ghKscBPCz5cDlWU0sGT5DYrWfoivz7ow6RyJynOL0JhWjHspCdMR"
    "6oKtDZgpRBE4LwGvcvlcY0VvHcqQwdszOjFVchSlTgSgVvVQ==";

int main(void)
{
    const int typemask = LIBSSH2_KNOWNHOST_TYPE_PLAIN |
                         LIBSSH2_KNOWNHOST_KEYENC_BASE64 |
                         LIBSSH2_KNOWNHOST_KEY_SSHRSA;
    /* An IPv6 literal for the default port, as OpenSSH stores it, followed
       by a regular RSA host key. */
    char line[512];
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;
    int rc;

    snprintf(line, sizeof(line), "[::1] ssh-rsa %s", key);

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

    rc = libssh2_knownhost_readline(hosts, line, strlen(line),
                                    LIBSSH2_KNOWNHOST_FILE_OPENSSH);
    if(rc) {
        fprintf(stderr, "libssh2_knownhost_readline failed: %d\n", rc);
        libssh2_knownhost_free(hosts);
        libssh2_session_free(session);
        libssh2_exit();
        return 1;
    }

    /* the same address in a different notation, with an explicit port */
    rc = libssh2_knownhost_check(hosts, "[::01]:2222", key, strlen(key),
                                 typemask, NULL);
    printf("knownhost check rc=%d (%s)\n", rc,
           rc == LIBSSH2_KNOWNHOST_CHECK_MATCH ? "match" :
           rc == LIBSSH2_KNOWNHOST_CHECK_MISMATCH ? "mismatch" : "notfound");

    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    libssh2_exit();

    return 0;
}
