/* Drives libssh2_knownhost_readline() on a crafted OpenSSH known_hosts
   line whose single hostname is 256 bytes long. */
#include <libssh2.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;
    static const char keypart[] = "ssh-rsa AAAAABBBBBCC";
    char *line;
    size_t off = 0;
    int rc;
    int i;

    if(libssh2_init(0))
        return 2;

    session = libssh2_session_init();
    if(!session)
        return 2;

    hosts = libssh2_knownhost_init(session);
    if(!hosts)
        return 2;

    /* <host> <key-type> <key>, the host part is 256 'a' bytes */
    line = malloc(256 + 1 + 20);
    if(!line)
        return 2;

    for(i = 0; i < 256; i++)
        line[off++] = 'a';
    line[off++] = ' ';
    memcpy(line + off, keypart, 20);
    off += 20;

    rc = libssh2_knownhost_readline(hosts, line, off,
                                    LIBSSH2_KNOWNHOST_FILE_OPENSSH);
    printf("readline rc=%d\n", rc);

    free(line);
    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    libssh2_exit();
    return 0;
}
