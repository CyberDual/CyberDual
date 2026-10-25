#include <curl/curl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t sink(char *ptr, size_t size, size_t nmemb, void *userp)
{
  (void)ptr;
  (void)userp;
  return size * nmemb;
}

int main(void)
{
  struct sockaddr_un addr;
  char response[256];
  char request[1024];
  int fd, client;
  pid_t child;
  CURL *curl;
  size_t n;
  int status;

  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  snprintf(addr.sun_path, sizeof(addr.sun_path), "/tmp/curl-header-%ld.sock", (long)getpid());
  unlink(addr.sun_path);
  fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if(fd < 0 || bind(fd, (struct sockaddr *)&addr, sizeof(addr)) || listen(fd, 1))
    return 2;

  child = fork();
  if(child < 0)
    return 2;
  if(!child) {
    client = accept(fd, NULL, NULL);
    if(client < 0)
      _exit(2);
    (void)read(client, request, sizeof(request));
    n = (size_t)snprintf(response, sizeof(response),
                         "HTTP/1.1 200 OK\r\nConnection: %s,x\r\nContent-Length: 0\r\n\r\n",
                         "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    if(write(client, response, n) != (ssize_t)n)
      _exit(2);
    close(client);
    close(fd);
    _exit(0);
  }

  curl = curl_easy_init();
  if(!curl)
    return 2;
  curl_easy_setopt(curl, CURLOPT_URL, "http://localhost/");
  curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, addr.sun_path);
  curl_easy_setopt(curl, CURLOPT_PROXY, "");
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, sink);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
  status = (int)curl_easy_perform(curl);
  curl_easy_cleanup(curl);
  waitpid(child, NULL, 0);
  close(fd);
  unlink(addr.sun_path);
  return status ? 1 : 0;
}
