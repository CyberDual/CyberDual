/* Drives a crafted HTTP/1.1 response through libcurl's public API using an
 * in-process loopback server. The response carries a Connection header whose
 * value is a long comma-separated option list. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <curl/curl.h>

static char g_response[4096];
static int g_port;

static void *server_thread(void *arg)
{
  int ls = *(int *)arg;
  int cs = accept(ls, NULL, NULL);
  if(cs >= 0) {
    char buf[4096];
    (void)read(cs, buf, sizeof(buf));
    (void)write(cs, g_response, strlen(g_response));
    close(cs);
  }
  return NULL;
}

static size_t writecb(char *ptr, size_t size, size_t nmemb, void *userdata)
{
  (void)ptr;
  (void)userdata;
  return size * nmemb;
}

int main(void)
{
  int ls;
  struct sockaddr_in sa;
  socklen_t salen = sizeof(sa);
  pthread_t tid;
  CURLcode rc;
  CURL *h;
  char url[64];
  size_t n;

  /* "Connection: abcde" followed by a long run of empty comma options */
  strcpy(g_response, "HTTP/1.1 200 OK\r\nConnection: abcde");
  n = strlen(g_response);
  memset(g_response + n, ',', 108);
  n += 108;
  strcpy(g_response + n, "\r\nContent-Length: 0\r\n\r\n");

  ls = socket(AF_INET, SOCK_STREAM, 0);
  memset(&sa, 0, sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  sa.sin_port = 0;
  if(bind(ls, (struct sockaddr *)&sa, sizeof(sa)) || listen(ls, 1) ||
     getsockname(ls, (struct sockaddr *)&sa, &salen)) {
    perror("server setup");
    return 2;
  }
  g_port = ntohs(sa.sin_port);
  if(pthread_create(&tid, NULL, server_thread, &ls)) {
    perror("pthread_create");
    return 2;
  }

  snprintf(url, sizeof(url), "http://127.0.0.1:%d/", g_port);
  curl_global_init(CURL_GLOBAL_DEFAULT);
  h = curl_easy_init();
  curl_easy_setopt(h, CURLOPT_URL, url);
  curl_easy_setopt(h, CURLOPT_NOPROXY, "*");
  curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, writecb);
  curl_easy_setopt(h, CURLOPT_TIMEOUT, 10L);
  rc = curl_easy_perform(h);
  fprintf(stderr, "rc=%d (%s)\n", rc, curl_easy_strerror(rc));
  curl_easy_cleanup(h);
  curl_global_cleanup();
  pthread_join(tid, NULL);
  close(ls);
  return 0;
}
