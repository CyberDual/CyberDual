#include <curl/curl.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

struct server {
  int fd;
};

static void *serve(void *arg)
{
  struct server *s = arg;
  int client = accept(s->fd, NULL, NULL);
  char buf[32768];
  size_t received = 0;
  size_t header_end = 0;
  size_t content_length = 0;

  if(client < 0)
    return NULL;
  {
    struct timeval timeout = { 5, 0 };
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  }
  while(!header_end) {
    ssize_t n = read(client, buf + received, sizeof(buf) - received);
    size_t i;
    if(n <= 0)
      goto done;
    received += (size_t)n;
    for(i = 4; i <= received; i++) {
      if(!memcmp(buf + i - 4, "\r\n\r\n", 4)) {
        header_end = i;
        break;
      }
    }
    if(received == sizeof(buf))
      goto done;
  }
  {
    buf[received] = 0;
    char *p = strstr(buf, "Content-Length: ");
    if(p)
      content_length = strtoull(p + 16, NULL, 10);
  }
  while(received - header_end < content_length) {
    ssize_t n = read(client, buf, sizeof(buf));
    if(n <= 0)
      goto done;
    received += (size_t)n;
  }
  {
    const char *response =
      "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    write(client, response, strlen(response));
  }
done:
  close(client);
  return NULL;
}

int main(int argc, char **argv)
{
  struct server s;
  struct sockaddr_in addr;
  socklen_t alen = sizeof(addr);
  pthread_t thread;
  CURL *easy;
  curl_mime *mime;
  curl_mimepart *part;
  struct curl_slist *headers = NULL;
  char url[128];
  char *data;
  char *name;
  size_t data_len = argc > 1 ? strtoull(argv[1], NULL, 10) : 12288;
  size_t name_len = argc > 2 ? strtoull(argv[2], NULL, 10) : 3;
  CURLcode result;

  s.fd = socket(AF_INET, SOCK_STREAM, 0);
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if(bind(s.fd, (struct sockaddr *)&addr, sizeof(addr)) ||
     listen(s.fd, 1) || getsockname(s.fd, (struct sockaddr *)&addr, &alen))
    return 2;
  if(pthread_create(&thread, NULL, serve, &s))
    return 2;
  snprintf(url, sizeof(url), "http://127.0.0.1:%u/", ntohs(addr.sin_port));
  data = malloc(data_len);
  name = malloc(name_len + 1);
  memset(data, 'a', data_len);
  memset(name, 'n', name_len);
  name[name_len] = 0;
  curl_global_init(CURL_GLOBAL_DEFAULT);
  easy = curl_easy_init();
  mime = curl_mime_init(easy);
  part = curl_mime_addpart(mime);
  curl_mime_name(part, name);
  curl_mime_data(part, data, data_len);
  curl_mime_encoder(part, "base64");
  headers = curl_slist_append(headers, "Expect:");
  curl_easy_setopt(easy, CURLOPT_URL, url);
  curl_easy_setopt(easy, CURLOPT_PROXY, "");
  curl_easy_setopt(easy, CURLOPT_MIMEPOST, mime);
  curl_easy_setopt(easy, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(easy, CURLOPT_UPLOAD_BUFFERSIZE, 16384L);
  curl_easy_setopt(easy, CURLOPT_TIMEOUT, 5L);
  result = curl_easy_perform(easy);
  fprintf(stderr, "curl result: %d\n", result);
  curl_slist_free_all(headers);
  curl_mime_free(mime);
  curl_easy_cleanup(easy);
  curl_global_cleanup();
  free(data);
  free(name);
  pthread_join(thread, NULL);
  close(s.fd);
  return result == CURLE_OK ? 0 : 1;
}
