#include <arpa/inet.h>
#include <curl/curl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

static size_t discard(char *ptr, size_t size, size_t count, void *userdata)
{
  (void)ptr;
  (void)userdata;
  return size * count;
}

int main(void)
{
  static const char response[] =
    "HTTP/1.1 200 OK\r\n"
    "Connection: keep-alive,   \r\n"
    "Content-Length: 0\r\n"
    "\r\n";
  struct sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);
  int listener = socket(AF_INET, SOCK_STREAM, 0);
  pid_t child;
  CURL *curl;
  CURLcode result;
  char url[80];

  if(listener < 0)
    return 2;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;
  if(bind(listener, (struct sockaddr *)&addr, sizeof(addr)) ||
     listen(listener, 1) ||
     getsockname(listener, (struct sockaddr *)&addr, &addrlen))
    return 2;

  child = fork();
  if(child < 0)
    return 2;
  if(child == 0) {
    char request[4096];
    int client = accept(listener, NULL, NULL);
    if(client >= 0) {
      size_t remaining = sizeof(response) - 1;
      const char *p = response;
      (void)read(client, request, sizeof(request));
      while(remaining) {
        ssize_t n = write(client, p, remaining);
        if(n <= 0)
          break;
        p += n;
        remaining -= (size_t)n;
      }
      close(client);
    }
    _exit(0);
  }

  close(listener);
  snprintf(url, sizeof(url), "http://127.0.0.1:%u/", ntohs(addr.sin_port));
  curl = curl_easy_init();
  if(!curl)
    return 2;
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_PROXY, "");
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discard);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
  result = curl_easy_perform(curl);
  curl_easy_cleanup(curl);
  waitpid(child, NULL, 0);
  return result == CURLE_OK ? 0 : 1;
}
