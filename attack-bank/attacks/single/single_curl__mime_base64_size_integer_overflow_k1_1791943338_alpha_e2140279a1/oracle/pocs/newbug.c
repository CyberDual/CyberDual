#include <curl/curl.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PAYLOAD 400

struct reader {
  const char *p;
  size_t left;
};

static size_t read_cb(char *ptr, size_t size, size_t nmemb, void *ud)
{
  struct reader *r = ud;
  size_t n = size * nmemb;

  if(n > r->left)
    n = r->left;
  if(n) {
    memcpy(ptr, r->p, n);
    r->p += n;
    r->left -= n;
  }
  return n;
}

static int listen_local(unsigned short *port)
{
  struct sockaddr_in addr;
  socklen_t alen = sizeof(addr);
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  int one = 1;

  if(fd < 0)
    return -1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;
  if(bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0 ||
     listen(fd, 1) < 0 ||
     getsockname(fd, (struct sockaddr *)&addr, &alen) < 0) {
    close(fd);
    return -1;
  }
  *port = ntohs(addr.sin_port);
  return fd;
}

static void serve(int lfd)
{
  int cfd = accept(lfd, NULL, NULL);
  char buf[8192];
  const char *resp =
    "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";

  if(cfd < 0)
    _exit(0);
  for(;;) {
    struct pollfd pfd;
    ssize_t n;

    pfd.fd = cfd;
    pfd.events = POLLIN;
    if(poll(&pfd, 1, 300) <= 0)
      break;
    n = recv(cfd, buf, sizeof(buf), 0);
    if(n <= 0)
      break;
  }
  send(cfd, resp, strlen(resp), MSG_NOSIGNAL);
  close(cfd);
  _exit(0);
}

int main(void)
{
  char body[PAYLOAD];
  char url[64];
  unsigned short port = 0;
  int lfd;
  pid_t kid;
  CURL *curl;
  curl_mime *mime;
  curl_mimepart *part;
  struct reader src;
  CURLcode rc;

  memset(body, 'A', sizeof(body));
  lfd = listen_local(&port);
  if(lfd < 0) {
    perror("listen");
    return 1;
  }
  kid = fork();
  if(kid < 0) {
    perror("fork");
    return 1;
  }
  if(kid == 0)
    serve(lfd);

  close(lfd);
  if(curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK)
    return 1;
  curl = curl_easy_init();
  if(!curl)
    return 1;
  mime = curl_mime_init(curl);
  part = curl_mime_addpart(mime);
  src.p = body;
  src.left = sizeof(body);
  curl_mime_name(part, "field");
  curl_mime_encoder(part, "base64");
  curl_mime_data_cb(part, (curl_off_t)sizeof(body), read_cb, NULL, NULL, &src);
  snprintf(url, sizeof(url), "http://127.0.0.1:%u/", port);
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_PROXY, "");
  curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  rc = curl_easy_perform(curl);
  curl_mime_free(mime);
  curl_easy_cleanup(curl);
  curl_global_cleanup();
  return rc == CURLE_OK ? 0 : 2;
}
