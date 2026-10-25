#include <string.h>

#include <libssh2.h>

/* OpenSSH known_hosts line: host token, then a key long enough to parse. */
static const char key[] = "01234567890123456789";

static int feed(LIBSSH2_KNOWNHOSTS *hosts, const char *line)
{
    return libssh2_knownhost_readline(hosts, line, strlen(line),
                                      LIBSSH2_KNOWNHOST_FILE_OPENSSH);
}

int main(void)
{
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;
    char plain[512];
    char hashed[512];
    char saltline[512];

    if(libssh2_init(0) != 0)
        return 1;

    session = libssh2_session_init();
    if(!session)
        return 1;

    hosts = libssh2_knownhost_init(session);
    if(!hosts)
        return 1;

    memset(plain, 'a', 255);
    plain[255] = ' ';
    memcpy(plain + 256, key, sizeof(key));

    memcpy(hashed, "|1|AAAA|", 8);
    memset(hashed + 8, 'b', 255);
    hashed[263] = ' ';
    memcpy(hashed + 264, key, sizeof(key));

    memcpy(saltline, "|1|", 3);
    memset(saltline + 3, 's', 31);
    saltline[34] = '|';
    memcpy(saltline + 35, "abcdefghij ", 11);
    memcpy(saltline + 46, key, sizeof(key));

    feed(hosts, plain);
    feed(hosts, hashed);
    feed(hosts, saltline);

    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    libssh2_exit();
    return 0;
}
