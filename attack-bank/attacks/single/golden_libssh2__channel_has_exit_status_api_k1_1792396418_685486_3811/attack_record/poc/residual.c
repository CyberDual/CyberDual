/* Copyright (C) The libssh2 project and its contributors.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <libssh2.h>

#include <string.h>

static size_t append_text(char *destination, size_t offset,
                          const char *text)
{
    size_t length = strlen(text);

    memcpy(destination + offset, text, length);
    return offset + length;
}

int main(void)
{
    LIBSSH2_SESSION *session;
    LIBSSH2_KNOWNHOSTS *hosts;
    char line[128];
    size_t offset = 0;
    int rc;

    if(libssh2_init(0))
        return 1;

    session = libssh2_session_init();
    if(!session) {
        libssh2_exit();
        return 1;
    }

    hosts = libssh2_knownhost_init(session);
    if(!hosts) {
        libssh2_session_free(session);
        libssh2_exit();
        return 1;
    }

    offset = append_text(line, offset, "|1|");
    offset = append_text(line, offset, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
    offset = append_text(line, offset, "|");
    offset = append_text(line, offset, "AAAAB3NzaC1yc2EAAAADAQABAAAB");
    offset = append_text(line, offset, " ");
    offset = append_text(line, offset,
                         "ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAAB");
    line[offset] = '\0';

    rc = libssh2_knownhost_readline(hosts, line, strlen(line),
                                     LIBSSH2_KNOWNHOST_FILE_OPENSSH);

    libssh2_knownhost_free(hosts);
    libssh2_session_free(session);
    libssh2_exit();

    return rc != 0;
}
