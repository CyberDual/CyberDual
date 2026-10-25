#include <libssh2.h>

#include <string.h>

int main(void)
{
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;
    char line[350];
    size_t pos = 0;
    const size_t label_lengths[] = { 62, 62, 62, 61 };
    const char suffix[] =
        "]:22 ssh-ed25519 "
        "AAAAC3NzaC1lZDI1NTE5AAAAIFEg0II3zbc1yWGhRzIc5KwTRlmx10RQP0sS63+1oMPi\n";
    size_t i;

    if(libssh2_init(0))
        return 1;
    session = libssh2_session_init();
    if(!session)
        return 2;
    hosts = libssh2_knownhost_init(session);
    if(!hosts)
        return 3;

    line[pos++] = '[';
    for(i = 0; i < sizeof(label_lengths) / sizeof(label_lengths[0]); i++) {
        memset(line + pos, 'a' + (int)i, label_lengths[i]);
        pos += label_lengths[i];
        if(i + 1 < sizeof(label_lengths) / sizeof(label_lengths[0]))
            line[pos++] = '.';
    }
    memcpy(line + pos, suffix, sizeof(suffix));
    pos += sizeof(suffix) - 1;

    libssh2_knownhost_readline(hosts, line, pos,
                               LIBSSH2_KNOWNHOST_FILE_OPENSSH);

    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    libssh2_exit();
    return 0;
}
