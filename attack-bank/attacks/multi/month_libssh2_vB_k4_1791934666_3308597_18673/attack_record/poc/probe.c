#include <libssh2.h>

#include <string.h>

int main(void)
{
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;
    char line[512];
    size_t hostlen = 255;
    static const char key[] = "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIE8m4CWnCTlF84YgHhZryUq2Ue0Pz0LSFmKjqaxZYsSV";

    if(libssh2_init(0))
        return 1;
    session = libssh2_session_init();
    if(!session)
        return 2;
    hosts = libssh2_knownhost_init(session);
    if(!hosts)
        return 3;

    memset(line, 'a', hostlen);
    line[0] = '[';
    line[51] = '.';
    line[102] = '.';
    line[153] = '.';
    line[204] = '.';
    line[hostlen - 6] = ']';
    memcpy(line + hostlen - 5, ":2222", 5);
    line[hostlen] = ' ';
    memcpy(line + hostlen + 1, key, sizeof(key));

    libssh2_knownhost_readline(hosts, line, hostlen + 1 + strlen(key),
                               LIBSSH2_KNOWNHOST_FILE_OPENSSH);

    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    libssh2_exit();
    return 0;
}
