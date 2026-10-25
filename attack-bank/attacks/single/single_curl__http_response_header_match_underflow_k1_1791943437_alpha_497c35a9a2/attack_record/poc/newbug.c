#include <curl/curl.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/* Response value longer than the single-token scratch, with no comma.
   A normal HTTP/1.1 response; nothing here is special besides length. */
#define TOKLEN 300

static size_t discard_cb(char *ptr, size_t size, size_t nmemb, void *userdata)
{
  (void)ptr;
  (void)userdata;
  return size * nmemb;
}

static void serve(int lfd)
{
  int cfd;
  char req[2048];
  char tok[TOKLEN + 1];
  char resp[TOKLEN + 128];
  int n;

  cfd = accept(lfd, NULL, NULL);
  if(cfd < 0)
    _exit(1);
  (void)recv(cfd, req, sizeof(req), 0);
  memset(tok, 'b', TOKLEN);
  tok[TOKLEN] = 0;
  n = snprintf(resp, sizeof(resp),
               "HTTP/1.1 200 OK\r\n"
               "Connection: %s\r\n"
               "Content-Length: 0\r\n"
               "\r\n",
               tok);
  if(n > 0)
    (void)send(cfd, resp, (size_t)n, 0);
  close(cfd);
  _exit(0);
}

int main(void)
{
  int lfd;
  struct sockaddr_in addr;
  socklen_t alen = sizeof(addr);
  pid_t pid;
  CURL *easy;
  char url[64];
  int port;
  CURLcode rc;

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

  pid = fork();
  if(pid < 0)
    return 1;
  if(pid == 0)
    serve(lfd);

  curl_global_init(CURL_GLOBAL_DEFAULT);
  easy = curl_easy_init();
  if(!easy)
    return 1;
  snprintf(url, sizeof(url), "http://127.0.0.1:%d/", port);
  curl_easy_setopt(easy, CURLOPT_URL, url);
  curl_easy_setopt(easy, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_1_1);
  curl_easy_setopt(easy, CURLOPT_PROXY, "");
  curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, discard_cb);
  curl_easy_setopt(easy, CURLOPT_TIMEOUT, 5L);
  curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT, 5L);
  rc = curl_easy_perform(easy);
  curl_easy_cleanup(easy);
  curl_global_cleanup();
  close(lfd);
  (void)rc;
  return 0;
}
