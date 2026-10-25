/*
 * Drive libssh2_session_handshake() from a socketpair that plays the role
 * of the server, in the same way the libssh2 client fuzzer does, and feed
 * it a channel request after the banner exchange.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <libssh2.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>

static int write_all(int fd, const unsigned char *buf, size_t len)
{
    while(len > 0) {
        ssize_t n = write(fd, buf, len);
        if(n <= 0)
            return -1;
        buf += n;
        len -= (size_t)n;
    }
    return 0;
}

static void store_u32(unsigned char *p, unsigned long v)
{
    p[0] = (unsigned char)((v >> 24) & 0xff);
    p[1] = (unsigned char)((v >> 16) & 0xff);
    p[2] = (unsigned char)((v >> 8) & 0xff);
    p[3] = (unsigned char)(v & 0xff);
}

int main(void)
{
    static const char banner[] = "SSH-2.0-libssh2_poc\r\n";

    /* SSH_MSG_CHANNEL_REQUEST carrying an "exit-status" request */
    unsigned char payload[21];
    unsigned char packet[5 + sizeof(payload)];
    int fds[2];
    LIBSSH2_SESSION *session;

    payload[0] = 98;                         /* SSH_MSG_CHANNEL_REQUEST */
    store_u32(payload + 1, 0);               /* recipient channel */
    store_u32(payload + 5, 11);              /* request type length */
    memcpy(payload + 9, "exit-status", 11);  /* request type */
    payload[20] = 0;                         /* want reply */

    store_u32(packet, 1 + sizeof(payload));  /* packet length */
    packet[4] = 0;                           /* padding length */
    memcpy(packet + 5, payload, sizeof(payload));

    if(libssh2_init(0)) {
        fprintf(stderr, "libssh2_init() failed\n");
        return 1;
    }

    if(socketpair(AF_UNIX, SOCK_STREAM, 0, fds)) {
        fprintf(stderr, "socketpair() failed\n");
        libssh2_exit();
        return 1;
    }

    if(write_all(fds[1], (const unsigned char *)banner,
                 sizeof(banner) - 1) ||
       write_all(fds[1], packet, sizeof(packet))) {
        fprintf(stderr, "write() failed\n");
        close(fds[0]);
        close(fds[1]);
        libssh2_exit();
        return 1;
    }

    shutdown(fds[1], SHUT_WR);

    session = libssh2_session_init();
    if(!session) {
        fprintf(stderr, "libssh2_session_init() failed\n");
        close(fds[0]);
        close(fds[1]);
        libssh2_exit();
        return 1;
    }

    libssh2_session_set_blocking(session, 1);

    /* The peer never completes the key exchange; the return value is not
       interesting for this run. */
    (void)libssh2_session_handshake(session, fds[0]);

    libssh2_session_free(session);
    libssh2_exit();
    close(fds[0]);
    close(fds[1]);

    return 0;
}
