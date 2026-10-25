#include <stdio.h>
#include <curl/curl.h>

/*
 * Set a legacy numeric IPv4 host the same way an application would, then read
 * it back. "0" is the 32-bit form of 0.0.0.0 and is accepted by curl_url_set().
 */
int main(void)
{
  CURLU *u;
  CURLUcode uc;
  char *host = NULL;

  u = curl_url();
  if(!u)
    return 1;

  uc = curl_url_set(u, CURLUPART_SCHEME, "http", 0);
  if(uc) {
    curl_url_cleanup(u);
    return 1;
  }

  uc = curl_url_set(u, CURLUPART_HOST, "0", 0);
  if(uc) {
    fprintf(stderr, "curl_url_set(HOST) -> %d\n", (int)uc);
    curl_url_cleanup(u);
    return 1;
  }

  uc = curl_url_get(u, CURLUPART_HOST, &host, 0);
  if(uc) {
    fprintf(stderr, "curl_url_get(HOST) -> %d\n", (int)uc);
    curl_url_cleanup(u);
    return 1;
  }

  printf("host=%s\n", host);
  curl_free(host);
  curl_url_cleanup(u);
  return 0;
}
