#include <curl/curl.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define TOKEN_LEN 47

static size_t sink(char *ptr, size_t size, size_t nmemb, void *userdata)
{
  (void)ptr;
  (void)userdata;
  return size * nmemb;
}

static int serve(int fd, const char *resp)
{
  char req[2048];
  size_t got = 0;
  size_t len;
  size_t off;

  while(got + 1 < sizeof(req)) {
    ssize_t n = recv(fd, req + got, sizeof(req) - 1 - got, 0);
    if(n <= 0)
      break;
    got += (size_t)n;
    req[got] = 0;
    if(strstr(req, "\r\n\r\n"))
      break;
  }

  len = strlen(resp);
  off = 0;
  while(off < len) {
    ssize_t n = send(fd, resp + off, len - off, 0);
    if(n < 0)
      return 1;
    off += (size_t)n;
  }
  shutdown(fd, SHUT_WR);
  return 0;
}

int main(void)
{
  int lfd;
  struct sockaddr_in addr;
  socklen_t alen;
  char token[TOKEN_LEN + 1];
  char conn[96];
  char resp[256];
  char url[64];
  pid_t pid;
  CURL *easy;
  CURLcode rc;
  int st = 0;

  memset(token, 'x', TOKEN_LEN);
  token[TOKEN_LEN] = 0;
  snprintf(conn, sizeof(conn), "Connection: %s, \r\n", token);
  if(strlen(conn) != 63) {
    fprintf(stderr, "unexpected header length %zu\n", strlen(conn));
    return 2;
  }
  snprintf(resp, sizeof(resp),
           "HTTP/1.1 200 OK\r\n"
           "%s"
           "Content-Length: 0\r\n"
           "\r\n",
           conn);

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
  alen = sizeof(addr);
  if(getsockname(lfd, (struct sockaddr *)&addr, &alen) < 0)
    return 1;

  pid = fork();
  if(pid < 0)
    return 1;
  if(pid == 0) {
    int cfd = accept(lfd, NULL, NULL);
    if(cfd >= 0) {
      serve(cfd, resp);
      close(cfd);
    }
    _exit(0);
  }
  close(lfd);

  snprintf(url, sizeof(url), "http://127.0.0.1:%d/", ntohs(addr.sin_port));
  curl_global_init(CURL_GLOBAL_DEFAULT);
  easy = curl_easy_init();
  if(!easy)
    return 1;
  curl_easy_setopt(easy, CURLOPT_URL, url);
  curl_easy_setopt(easy, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_1_1);
  curl_easy_setopt(easy, CURLOPT_PROXY, "");
  curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, sink);
  curl_easy_setopt(easy, CURLOPT_TIMEOUT, 10L);
  curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L);
  rc = curl_easy_perform(easy);
  fprintf(stderr, "curl: %s\n", curl_easy_strerror(rc));
  fflush(stderr);
  curl_easy_cleanup(easy);
  curl_global_cleanup();
  waitpid(pid, &st, 0);
  return (rc == CURLE_OK) ? 0 : 1;
}
