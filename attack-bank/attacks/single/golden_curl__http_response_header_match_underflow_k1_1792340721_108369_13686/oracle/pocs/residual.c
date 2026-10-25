#include <arpa/inet.h>
#include <curl/curl.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main(void)
{
  CURLcode result;
  CURL *easy;
  struct curl_slist *headers = NULL;
  struct sockaddr_in address;
  socklen_t address_len = sizeof(address);
  int listener;
  uint16_t port;
  char url[64];

  listener = socket(AF_INET, SOCK_STREAM, 0);
  if(listener < 0)
    return 1;

  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = 0;
  if(bind(listener, (struct sockaddr *)&address, sizeof(address)) ||
     listen(listener, 1) ||
     getsockname(listener, (struct sockaddr *)&address, &address_len)) {
    close(listener);
    return 1;
  }

  port = ntohs(address.sin_port);
  snprintf(url, sizeof(url), "http://127.0.0.1:%u/", (unsigned int)port);

  curl_global_init(CURL_GLOBAL_DEFAULT);
  easy = curl_easy_init();
  if(!easy) {
    close(listener);
    curl_global_cleanup();
    return 1;
  }

  headers = curl_slist_append(headers, "Transfer-Encoding: chunked");
  if(!headers) {
    close(listener);
    curl_easy_cleanup(easy);
    curl_global_cleanup();
    return 1;
  }

  curl_easy_setopt(easy, CURLOPT_URL, url);
  curl_easy_setopt(easy, CURLOPT_PROXY, "");
  curl_easy_setopt(easy, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(easy, CURLOPT_NOBODY, 1L);
  curl_easy_setopt(easy, CURLOPT_TIMEOUT_MS, 1000L);

  result = curl_easy_perform(easy);
  printf("curl_easy_perform: %s\n", curl_easy_strerror(result));

  curl_slist_free_all(headers);
  curl_easy_cleanup(easy);
  curl_global_cleanup();
  close(listener);
  return 0;
}
