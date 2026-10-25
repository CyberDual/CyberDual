/* Copyright (C) The libssh2 project and its contributors.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Standalone reproducer for the channel "exit-status" request handling.
 *
 * The server side of the socketpair feeds the client a banner, a KEXINIT,
 * an X11 channel open (accepted through the client's X11 callback so a
 * real channel exists) and finally an SSH_MSG_CHANNEL_REQUEST
 * "exit-status" request for that channel. The request carries the request
 * string and the want-reply byte but stops right before the status code,
 * so the payload buffer ends exactly where the status field is expected
 * to start. The request is consumed while the key exchange is still
 * waiting for the server reply. Run against a libssh2 build with
 * AddressSanitizer enabled.
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "libssh2.h"

#define SSH_MSG_KEXINIT 20
#define SSH_MSG_CHANNEL_OPEN 90
#define SSH_MSG_CHANNEL_REQUEST 98

static unsigned char wire[4096];
static size_t wire_len;

static unsigned char payload[2048];
static size_t payload_len;

static void wire_write(const void *p, size_t n)
{
    memcpy(wire + wire_len, p, n);
    wire_len += n;
}

static void payload_write(const void *p, size_t n)
{
    memcpy(payload + payload_len, p, n);
    payload_len += n;
}

static void payload_u32(uint32_t v)
{
    unsigned char b[4];

    b[0] = (unsigned char)((v >> 24) & 0xff);
    b[1] = (unsigned char)((v >> 16) & 0xff);
    b[2] = (unsigned char)((v >> 8) & 0xff);
    b[3] = (unsigned char)(v & 0xff);

    payload_write(b, sizeof(b));
}

static void payload_byte(unsigned char b)
{
    payload_write(&b, 1);
}

static void payload_str(const char *s)
{
    size_t n = strlen(s);

    payload_u32((uint32_t)n);
    payload_write(s, n);
}

/* Frame the pending payload as an unencrypted binary packet. */
static void frame_packet(void)
{
    unsigned char header[5];
    uint32_t packet_length = (uint32_t)(payload_len + 1);

    header[0] = (unsigned char)((packet_length >> 24) & 0xff);
    header[1] = (unsigned char)((packet_length >> 16) & 0xff);
    header[2] = (unsigned char)((packet_length >> 8) & 0xff);
    header[3] = (unsigned char)(packet_length & 0xff);
    header[4] = 0; /* padding length */

    wire_write(header, sizeof(header));
    wire_write(payload, payload_len);
}

static void build_kexinit(void)
{
    static const char *const name_lists[] = {
        "curve25519-sha256,ecdh-sha2-nistp256,"
        "diffie-hellman-group14-sha256,diffie-hellman-group-exchange-sha256",
        "ssh-ed25519,ecdsa-sha2-nistp256,rsa-sha2-512,rsa-sha2-256,ssh-rsa",
        "aes128-ctr,aes256-ctr,aes128-cbc,aes256-cbc",
        "aes128-ctr,aes256-ctr,aes128-cbc,aes256-cbc",
        "hmac-sha2-256,hmac-sha2-512,hmac-sha1",
        "hmac-sha2-256,hmac-sha2-512,hmac-sha1",
        "none",
        "none",
        "",
        "",
    };
    static const unsigned char cookie[16];
    size_t i;

    payload_len = 0;
    payload_byte(SSH_MSG_KEXINIT);
    payload_write(cookie, sizeof(cookie));
    for(i = 0; i < sizeof(name_lists) / sizeof(name_lists[0]); i++)
        payload_str(name_lists[i]);
    payload_byte(0); /* first_kex_packet_follows */
    payload_u32(0); /* reserved */

    frame_packet();
}

static void build_x11_open(uint32_t sender_channel)
{
    payload_len = 0;
    payload_byte(SSH_MSG_CHANNEL_OPEN);
    payload_str("x11");
    payload_u32(sender_channel);
    payload_u32(0x20000); /* initial window size */
    payload_u32(0x4000); /* maximum packet size */
    payload_str("127.0.0.1");
    payload_u32(6000);

    frame_packet();
}

static void build_exit_status_request(uint32_t channel, int include_status)
{
    payload_len = 0;
    payload_byte(SSH_MSG_CHANNEL_REQUEST);
    payload_u32(channel);
    payload_str("exit-status");
    payload_byte(0); /* want reply */
    if(include_status)
        payload_u32(7);

    frame_packet();
}

static void x11_open_cb(LIBSSH2_CHANNEL *channel, char *shost, int sport,
                        void **abstract)
{
    (void)channel;
    (void)shost;
    (void)sport;
    (void)abstract;
}

int main(void)
{
    static const char banner[] = "SSH-2.0-ExitStatusReproducer\r\n";
    int socket_fds[2] = { -1, -1 };
    LIBSSH2_SESSION *session = NULL;
    ssize_t written;
    int rc;

    rc = libssh2_init(0);
    if(rc) {
        fprintf(stderr, "libssh2_init() failed (%d)\n", rc);
        return 1;
    }

    if(socketpair(AF_UNIX, SOCK_STREAM, 0, socket_fds)) {
        fprintf(stderr, "socketpair() failed (%s)\n", strerror(errno));
        libssh2_exit();
        return 1;
    }

    wire_len = 0;
    wire_write(banner, sizeof(banner) - 1);
    build_kexinit();
    build_x11_open(0x100); /* becomes local channel 0 */
    build_exit_status_request(0, 0);

    written = send(socket_fds[1], wire, wire_len, 0);
    if(written != (ssize_t)wire_len) {
        fprintf(stderr, "send() of %lu bytes returned %ld (%s)\n",
                (unsigned long)wire_len, (long)written, strerror(errno));
        goto cleanup;
    }

    if(shutdown(socket_fds[1], SHUT_WR)) {
        fprintf(stderr, "shutdown() failed (%s)\n", strerror(errno));
        goto cleanup;
    }

    session = libssh2_session_init();
    if(!session) {
        fprintf(stderr, "libssh2_session_init() failed\n");
        goto cleanup;
    }

    libssh2_session_set_blocking(session, 1);
    libssh2_session_callback_set2(session, LIBSSH2_CALLBACK_X11,
                                  (libssh2_cb_generic *)x11_open_cb);

    /* The channel request is parsed while the key exchange waits for the
       server side to answer. */
    rc = libssh2_session_handshake(session, socket_fds[0]);
    fprintf(stderr, "libssh2_session_handshake() returned %d\n", rc);

    libssh2_session_free(session);

cleanup:
    libssh2_exit();
    close(socket_fds[0]);
    close(socket_fds[1]);

    return 0;
}
