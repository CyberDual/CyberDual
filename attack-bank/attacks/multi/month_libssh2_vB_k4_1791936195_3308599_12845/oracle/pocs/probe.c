#include <libssh2.h>

#include <string.h>

int main(void)
{
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;
    struct libssh2_knownhost *found;
    static const char line[] =
        "example.com ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIAtest1234567890\n";
    static const char evil_host[] = "1:2:3:4:5:6:7:1.2.3.4";
    static const char key[] = "AAAAC3NzaC1lZDI1NTE5AAAAIAtest1234567890";

    if(libssh2_init(0))
        return 1;

    session = libssh2_session_init();
    if(!session)
        return 1;

    hosts = libssh2_knownhost_init(session);
    if(!hosts)
        return 1;

    if(libssh2_knownhost_readline(hosts, line, strlen(line),
                                   LIBSSH2_KNOWNHOST_FILE_OPENSSH))
        return 1;

    /* Compare a crafted plain-text host entry against the known_hosts
       collection. The host string is a syntactically valid IPv6 literal
       with 7 leading 16-bit groups followed by an embedded IPv4 tail. */
    libssh2_knownhost_check(hosts, evil_host, key, strlen(key),
                             LIBSSH2_KNOWNHOST_TYPE_PLAIN |
                             LIBSSH2_KNOWNHOST_KEYENC_BASE64,
                             &found);

    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    libssh2_exit();
    return 0;
}
