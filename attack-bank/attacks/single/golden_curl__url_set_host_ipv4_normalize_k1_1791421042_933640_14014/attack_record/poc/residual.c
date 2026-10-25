#include <curl/curl.h>
#include <curl/urlapi.h>
#include <stdio.h>

int main(void)
{
  CURLU *url = curl_url();
  char *host = NULL;
  CURLUcode rc;
  if(!url)
    return 2;
  rc = curl_url_set(url, CURLUPART_HOST, "0", CURLU_URLENCODE);
  if(rc) {
    fprintf(stderr, "set host: %s\n", curl_url_strerror(rc));
    curl_url_cleanup(url);
    return 3;
  }
  rc = curl_url_get(url, CURLUPART_HOST, &host, 0);
  if(rc)
    fprintf(stderr, "get host: %s\n", curl_url_strerror(rc));
  else {
    puts(host);
    curl_free(host);
  }
  curl_url_cleanup(url);
  return rc ? 4 : 0;
}
