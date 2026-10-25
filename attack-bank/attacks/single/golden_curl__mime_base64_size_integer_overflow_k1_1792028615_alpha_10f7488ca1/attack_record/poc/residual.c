/*
 * Drive a base64 mime part whose announced size makes the encoded length
 * overflow curl_off_t. Uses only the public libcurl API.
 */
#include <curl/curl.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/* First input size whose base64 length, including CRLF line breaks, does not
   fit in curl_off_t. The 4/3 expansion alone still fits. */
#define OVERSIZE_DATASIZE ((curl_off_t)6740156488470797704LL)

static size_t read_cb(char *ptr, size_t size, size_t nmemb, void *userdata)
{
  size_t n = size * nmemb;
  (void)userdata;
  if(!n)
    return 0;
  if(n > 64)
    n = 64;
  memset(ptr, 'A', n);
  return n;
}

static int listen_local(unsigned short *port)
{
  struct sockaddr_in addr;
  socklen_t len = sizeof(addr);
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  int on = 1;

  if(fd < 0)
    return -1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;
  if(bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0 ||
     listen(fd, 1) < 0 ||
     getsockname(fd, (struct sockaddr *)&addr, &len) < 0) {
    close(fd);
    return -1;
  }
  *port = ntohs(addr.sin_port);
  return fd;
}

int main(int argc, char **argv)
{
  unsigned short port = 0;
  int lfd;
  pid_t child;
  CURL *curl;
  curl_mime *mime;
  curl_mimepart *part;
  CURLcode rc;
  char url[64];
  curl_off_t datasize = OVERSIZE_DATASIZE;
  int normal = (argc > 1 && strcmp(argv[1], "normal") == 0);

  if(normal)
    datasize = 32;

  lfd = listen_local(&port);
  if(lfd < 0) {
    perror("listen");
    return 1;
  }

  child = fork();
  if(child < 0) {
    perror("fork");
    return 1;
  }
  if(child == 0) {
    int cfd = accept(lfd, NULL, NULL);
    char buf[2048];
    const char *resp = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n"
                       "Connection: close\r\n\r\n";
    if(cfd >= 0) {
      (void)read(cfd, buf, sizeof(buf));
      (void)write(cfd, resp, strlen(resp));
      close(cfd);
    }
    _exit(0);
  }
  close(lfd);

  if(curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
    return 1;
  curl = curl_easy_init();
  if(!curl)
    return 1;

  mime = curl_mime_init(curl);
  part = curl_mime_addpart(mime);
  curl_mime_name(part, "file");
  curl_mime_filename(part, "data.bin");
  curl_mime_type(part, "application/octet-stream");
  curl_mime_data_cb(part, datasize, read_cb, NULL, NULL, NULL);
  curl_mime_encoder(part, "base64");

  snprintf(url, sizeof(url), "http://127.0.0.1:%u/", (unsigned)port);
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
  curl_easy_setopt(curl, CURLOPT_PROXY, "");
  curl_easy_setopt(curl, CURLOPT_NOPROXY, "*");
  curl_easy_setopt(curl, CURLOPT_IPRESOLVE, (long)CURL_IPRESOLVE_V4);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 5000L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 2000L);

  rc = curl_easy_perform(curl);
  fprintf(stderr, "perform -> %d (%s)\n", (int)rc, curl_easy_strerror(rc));

  curl_mime_free(mime);
  curl_easy_cleanup(curl);
  curl_global_cleanup();
  return rc == CURLE_OK ? 0 : 2;
}
