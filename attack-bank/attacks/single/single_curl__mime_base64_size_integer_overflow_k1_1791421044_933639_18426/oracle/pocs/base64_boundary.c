#include <curl/curl.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static int listener;

static void *serve(void *unused)
{
  int fd = accept(listener, NULL, NULL);
  char buf[32768];
  size_t used = 0;
  (void)unused;
  if(fd < 0)
    return NULL;
  for(;;) {
    char *head;
    char *cl;
    size_t length;
    ssize_t n = recv(fd, buf + used, sizeof(buf) - used - 1, 0);
    if(n <= 0)
      break;
    used += (size_t)n;
    buf[used] = 0;
    head = strstr(buf, "\r\n\r\n");
    if(!head)
      continue;
    cl = strstr(buf, "Content-Length:");
    length = cl && cl < head ? (size_t)strtoul(cl + 15, NULL, 10) : 0;
    if(used >= (size_t)(head + 4 - buf) + length)
      break;
  }
  {
    const char reply[] =
      "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    send(fd, reply, sizeof(reply) - 1, 0);
  }
  close(fd);
  return NULL;
}

static size_t discard(char *ptr, size_t size, size_t count, void *arg)
{
  (void)ptr;
  (void)arg;
  return size * count;
}

int main(void)
{
  struct sockaddr_un address;
  pthread_t thread;
  char path[] = "/tmp/curl-mime-XXXXXX";
  char data[11900];
  CURL *easy;
  curl_mime *mime;
  curl_mimepart *part;
  int fd = mkstemp(path);
  if(fd < 0)
    return 2;
  close(fd);
  unlink(path);
  listener = socket(AF_UNIX, SOCK_STREAM, 0);
  memset(&address, 0, sizeof(address));
  address.sun_family = AF_UNIX;
  memcpy(address.sun_path, path, strlen(path) + 1);
  if(listener < 0 ||
     bind(listener, (struct sockaddr *)&address, sizeof(address)) ||
     listen(listener, 1))
    return 2;
  if(pthread_create(&thread, NULL, serve, NULL))
    return 2;
  memset(data, 'A', sizeof(data));
  if(curl_global_init(CURL_GLOBAL_DEFAULT))
    return 2;
  easy = curl_easy_init();
  mime = curl_mime_init(easy);
  part = curl_mime_addpart(mime);
  curl_mime_name(part, "f");
  curl_mime_data(part, data, sizeof(data));
  curl_mime_encoder(part, "base64");
  curl_easy_setopt(easy, CURLOPT_URL, "http://localhost/");
  curl_easy_setopt(easy, CURLOPT_UNIX_SOCKET_PATH, path);
  curl_easy_setopt(easy, CURLOPT_PROXY, "");
  curl_easy_setopt(easy, CURLOPT_MIMEPOST, mime);
  curl_easy_setopt(easy, CURLOPT_UPLOAD_BUFFERSIZE, 16384L);
  curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, discard);
  curl_easy_setopt(easy, CURLOPT_TIMEOUT_MS, 2000L);
  if(curl_easy_perform(easy))
    return 2;
  curl_easy_cleanup(easy);
  curl_mime_free(mime);
  curl_global_cleanup();
  pthread_join(thread, NULL);
  close(listener);
  unlink(path);
  return 0;
}
