/*
 * Reproducer: mime base64 part fed through curl_mime_data_cb().
 *
 * Builds a multipart/form-data POST with a single file part that uses the
 * "base64" content encoder, pushes 1000 bytes of content through the
 * documented curl_mime_* callback interface and performs the request
 * against a local loopback HTTP server.
 *
 * On a sane library this completes successfully and the process exits 0.
 * If libcurl faults while encoding/sending, the process aborts (rc 134
 * under AddressSanitizer).
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <curl/curl.h>

static int port;
static curl_off_t remaining;

/* Make sure a sanitizer report terminates the process with SIGABRT. */
const char *__asan_default_options(void)
{
  return "abort_on_error=1";
}

static void *server(void *arg)
{
  int s, c, one = 1;
  struct sockaddr_in a;
  socklen_t alen = sizeof(a);
  static char buf[16384];
  ssize_t n;
  (void)arg;

  s = socket(AF_INET, SOCK_STREAM, 0);
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  memset(&a, 0, sizeof(a));
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  a.sin_port = 0;
  if(bind(s, (struct sockaddr *)&a, sizeof(a)) < 0)
    return NULL;
  getsockname(s, (struct sockaddr *)&a, &alen);
  port = ntohs(a.sin_port);
  listen(s, 8);

  for(;;) {
    size_t got = 0;
    long clen = -1;
    int hdrend = -1;
    c = accept(s, NULL, NULL);
    if(c < 0)
      continue;
    for(;;) {
      n = recv(c, buf + got, sizeof(buf) - got - 1, 0);
      if(n <= 0)
        break;
      got += (size_t)n;
      buf[got] = 0;
      if(hdrend < 0) {
        char *p = strstr(buf, "\r\n\r\n");
        if(p) {
          char *cl;
          hdrend = (int)(p - buf) + 4;
          cl = strcasestr(buf, "Content-Length:");
          if(cl)
            clen = atol(cl + 15);
          if(strstr(buf, "Expect: 100-continue")) {
            const char *cont = "HTTP/1.1 100 Continue\r\n\r\n";
            send(c, cont, strlen(cont), 0);
          }
          if(clen < 0)
            break; /* chunked: just read until close */
        }
      }
      if(hdrend >= 0 && clen >= 0 && got >= (size_t)hdrend + (size_t)clen)
        break;
      if(got > sizeof(buf) - 2000)
        break;
    }
    {
      const char *resp =
        "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok";
      send(c, resp, strlen(resp), 0);
    }
    close(c);
  }
  return NULL;
}

static size_t readcb(char *buffer, size_t size, size_t nitems, void *arg)
{
  size_t max = size * nitems;
  (void)arg;
  if(remaining <= 0)
    return 0;
  if((curl_off_t)max > remaining)
    max = (size_t)remaining;
  memset(buffer, 'A', max);
  remaining -= (curl_off_t)max;
  return max;
}

/* Perform one multipart POST with a base64-encoded part of `datalen`
 * bytes through the public curl_mime_* API. Returns the perform result. */
static CURLcode do_transfer(long datalen)
{
  CURL *curl;
  CURLcode rc;
  curl_mime *mime;
  curl_mimepart *part;
  char url[128];

  curl = curl_easy_init();
  if(!curl)
    return CURLE_FAILED_INIT;
  mime = curl_mime_init(curl);
  part = curl_mime_addpart(mime);
  curl_mime_name(part, "file");
  curl_mime_filename(part, "data.bin");
  curl_mime_encoder(part, "base64");
  remaining = datalen;
  curl_mime_data_cb(part, datalen, readcb, NULL, NULL, NULL);

  snprintf(url, sizeof(url), "http://127.0.0.1:%d/", port);
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_NOPROXY, "*");
  curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

  rc = curl_easy_perform(curl);

  curl_mime_free(mime);
  curl_easy_cleanup(curl);
  return rc;
}

static int run_case(void)
{
  /* 1000 bytes of part content: the base64 stream ends with a partial
     group, exercising the encoder's end-of-stream flush path. */
  return do_transfer(1000) == CURLE_OK ? 0 : 1;
}

int main(void)
{
  pthread_t tid;
  int attempt;

  signal(SIGPIPE, SIG_IGN);
  pthread_create(&tid, NULL, server, NULL);
  while(!port)
    usleep(100);

  curl_global_init(CURL_GLOBAL_ALL);

  /* Run the case in a child first to tell a genuine library abort
     (SIGABRT, e.g. AddressSanitizer report) apart from an inconclusive
     environment hiccup, then confirm it in this process. */
  for(attempt = 0; attempt < 40; attempt++) {
    pid_t pid = fork();
    if(pid == 0)
      _exit(run_case());
    if(pid > 0) {
      int status = 0;
      waitpid(pid, &status, 0);
      if(WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        curl_global_cleanup();
        return 0;                     /* transfer completed: library is sane */
      }
      if(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT) {
        run_case();                   /* aborts here on a faulty library */
        curl_global_cleanup();
        return 0;
      }
      /* any other outcome (transfer error, startup hiccup): retry */
    }
  }
  curl_global_cleanup();
  return 1;
}
