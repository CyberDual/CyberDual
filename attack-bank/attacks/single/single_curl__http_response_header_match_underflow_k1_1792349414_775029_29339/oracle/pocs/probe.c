/* Test harness: drives a crafted HTTP/1.1 response header through libcurl's
 * public API using an in-process loopback server (forked child). */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <curl/curl.h>

static char g_response[65536];

static size_t writecb(char *ptr, size_t size, size_t nmemb, void *userdata)
{
  (void)ptr;
  (void)userdata;
  return size * nmemb;
}

int main(int argc, char **argv)
{
  static char value[60000];
  const char *hdr = argc > 1 ? argv[1] : "Connection";
  const char *value_in = argc > 2 ? argv[2] : "abcde,   ";
  size_t vlen = strlen(value_in);
  int ls, port, st;
  struct sockaddr_in sa;
  socklen_t salen = sizeof(sa);
  pid_t pid;
  CURLcode rc;
  CURL *h;
  char url[64];

  /* expand 'B<num>' runs into that many blanks */
  {
    size_t o = 0, i;
    for(i = 0; i < vlen && o < sizeof(value) - 1; i++) {
      if(value_in[i] == 'B' && i + 1 < vlen &&
         value_in[i + 1] >= '0' && value_in[i + 1] <= '9') {
        long n = strtol(&value_in[i + 1], NULL, 10);
        const char *p = &value_in[i + 1];
        long k;
        while(*p >= '0' && *p <= '9')
          p++;
        for(k = 0; k < n && o < sizeof(value) - 1; k++)
          value[o++] = ' ';
        i = (size_t)(p - value_in) - 1;
      }
      else
        value[o++] = value_in[i];
    }
    value[o] = 0;
  }

  snprintf(g_response, sizeof(g_response),
           "HTTP/1.1 200 OK\r\n"
           "%s: %s\r\n"
           "Content-Length: 0\r\n"
           "\r\n", hdr, value);

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
  port = ntohs(sa.sin_port);

  pid = fork();
  if(pid < 0) {
    perror("fork");
    return 2;
  }
  if(pid == 0) {
    /* child: serve a single response */
    int cs = accept(ls, NULL, NULL);
    if(cs >= 0) {
      char buf[4096];
      (void)read(cs, buf, sizeof(buf));
      (void)write(cs, g_response, strlen(g_response));
      close(cs);
    }
    _exit(0);
  }

  snprintf(url, sizeof(url), "http://127.0.0.1:%d/", port);
  curl_global_init(CURL_GLOBAL_DEFAULT);
  h = curl_easy_init();
  curl_easy_setopt(h, CURLOPT_URL, url);
  curl_easy_setopt(h, CURLOPT_NOPROXY, "*");
  curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, writecb);
  rc = curl_easy_perform(h);
  fprintf(stderr, "rc=%d (%s)\n", rc, curl_easy_strerror(rc));
  curl_easy_cleanup(h);
  curl_global_cleanup();
  close(ls);
  (void)waitpid(pid, &st, 0);
  return 0;
}
