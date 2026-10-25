#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

/* Legacy numerical IPv4 with a long run of leading zeroes. Full URL parsing
   accepts this and canonicalizes it; curl_url_set(CURLUPART_HOST) should too. */
int main(void)
{
  enum { N = 256 };
  char host[N + 1];
  CURLU *u;
  CURLUcode rc;
  char *out = NULL;

  memset(host, '0', N);
  host[N] = '\0';

  u = curl_url();
  if(!u)
    return 1;
  rc = curl_url_set(u, CURLUPART_HOST, host, 0);
  if(rc == CURLUE_OK)
    curl_url_get(u, CURLUPART_HOST, &out, 0);
  else
    fprintf(stderr, "curl_url_set: %s\n", curl_url_strerror(rc));
  if(out) {
    printf("%s\n", out);
    curl_free(out);
  }
  curl_url_cleanup(u);
  return rc == CURLUE_OK ? 0 : 2;
}
