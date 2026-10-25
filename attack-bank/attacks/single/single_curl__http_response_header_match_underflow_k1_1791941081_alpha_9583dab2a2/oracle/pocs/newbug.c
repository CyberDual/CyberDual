/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 * / __| | | | |_) | |
 * | (__| |_| |  _ <| |___
 *  \___|\___|_| \_\_____|
 *
 * Proof of concept for the response-header matcher regression.
 *
 * Build (against an AddressSanitizer-enabled libcurl):
 *   cc -g -fsanitize=address -fno-omit-frame-pointer \
 *      -I<curl-include> -o newbug newbug.c <libcurl.a> -lpthread -ldl -lm
 *
 * SPDX-License-Identifier: curl
 *
 ***************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <curl/curl.h>

#define PORT 18099

/* 40 blanks between the first token and a token that begins with "close" */
#define RESP_BODY \
  "HTTP/1.1 200 OK\r\n" \
  "Connection: x,                                        closeX\r\n" \
  "Content-Length: 0\r\n" \
  "\r\n"

static void *serve_once(void *arg)
{
  (void)arg;
  int srv = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in addr;
  int on = 1;
  char req[8192];

  setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(PORT);
  if(bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind");
    close(srv);
    return NULL;
  }
  listen(srv, 1);
  int conn = accept(srv, NULL, NULL);
  if(conn >= 0) {
    recv(conn, req, sizeof(req), 0);
    send(conn, RESP_BODY, sizeof(RESP_BODY) - 1, 0);
    close(conn);
  }
  close(srv);
  return NULL;
}

static size_t write_cb(void *ptr, size_t size, size_t nmemb, void *data)
{
  (void)ptr; (void)data;
  return size * nmemb;
}

int main(void)
{
  pthread_t tid;
  CURLcode rc;

  pthread_create(&tid, NULL, serve_once, NULL);
  usleep(100000);

  curl_global_init(CURL_GLOBAL_DEFAULT);
  CURL *curl = curl_easy_init();
  curl_easy_setopt(curl, CURLOPT_URL, "http://127.0.0.1:" "18099" "/");
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 10000L);
  rc = curl_easy_perform(curl);
  fprintf(stderr, "perform rc=%d (%s)\n", rc, curl_easy_strerror(rc));
  curl_easy_cleanup(curl);
  curl_global_cleanup();
  pthread_join(tid, NULL);
  return 0;
}
