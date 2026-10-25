#include <libssh2.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const int plain_type =
    LIBSSH2_KNOWNHOST_TYPE_PLAIN |
    LIBSSH2_KNOWNHOST_KEYENC_BASE64 |
    LIBSSH2_KNOWNHOST_KEY_SSHRSA;

static const char keymat[] = "ssh-rsa AAAAAAAAAAAAAAAAAAAAAAAA";

static int expect_match(LIBSSH2_SESSION *session,
                        const char *stored, const char *checked)
{
    LIBSSH2_KNOWNHOSTS *hosts;
    int rc;
    const char *key = "AAAA";

    hosts = libssh2_knownhost_init(session);
    if(!hosts)
        return 1;

    rc = libssh2_knownhost_addc(hosts, stored, NULL, key, strlen(key),
                                NULL, 0, plain_type, NULL);
    if(rc) {
        libssh2_knownhost_free(hosts);
        return 1;
    }

    rc = libssh2_knownhost_check(hosts, checked, key, strlen(key),
                                 plain_type, NULL);
    libssh2_knownhost_free(hosts);
    return rc != LIBSSH2_KNOWNHOST_CHECK_MATCH;
}

static void trigger(LIBSSH2_SESSION *session)
{
    LIBSSH2_KNOWNHOSTS *hosts;
    char line[640];
    char name[256];
    const char *bad = "::1:2:3:4:5:6:7:8:9";
    const char *key = "AAAA";

    hosts = libssh2_knownhost_init(session);
    if(!hosts)
        return;

    memset(name, 'a', 255);
    memcpy(line, name, 255);
    memcpy(line + 255, " ", 1);
    memcpy(line + 256, keymat, sizeof(keymat));
    libssh2_knownhost_readline(hosts, line, strlen(line),
                               LIBSSH2_KNOWNHOST_FILE_OPENSSH);

    memcpy(line, "|1|abcd|", 8);
    memset(line + 8, 'A', 255);
    memcpy(line + 263, " ", 1);
    memcpy(line + 264, keymat, sizeof(keymat));
    libssh2_knownhost_readline(hosts, line, strlen(line),
                               LIBSSH2_KNOWNHOST_FILE_OPENSSH);

    libssh2_knownhost_addc(hosts, "example.com", NULL, key, strlen(key),
                           NULL, 0, plain_type, NULL);
    libssh2_knownhost_check(hosts, bad, key, strlen(key), plain_type, NULL);

    libssh2_knownhost_free(hosts);
}

int main(void)
{
    LIBSSH2_SESSION *session;
    int rc;

    rc = libssh2_init(0);
    if(rc)
        return 1;

    session = libssh2_session_init();
    if(!session)
        return 1;

    /* Ordinary names and valid IPv6 literals stay on the public path. */
    if(expect_match(session,
                    "fd5f:4268:2387:97c1:0000:0000:0000:0002",
                    "fd5f:4268:2387:97c1::2") ||
       expect_match(session, "0:0:0:0:0:0:0:0", "::") ||
       expect_match(session, "2001:db8:1:2:3:4:0:0", "2001:db8:1:2:3:4::") ||
       expect_match(session, "example.com", "example.com")) {
        fprintf(stderr, "known-host match failed\n");
        return 2;
    }

    trigger(session);

    libssh2_session_free(session);
    libssh2_exit();
    return 0;
}
