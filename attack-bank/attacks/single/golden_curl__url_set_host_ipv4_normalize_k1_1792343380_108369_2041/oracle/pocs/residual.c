#include <stdio.h>
#include <curl/curl.h>

int main(void)
{
  CURLU *url = curl_url();
  CURLUcode result;

  if(!url)
    return 2;

  result = curl_url_set(url, CURLUPART_HOST, "0", 0);
  fprintf(stderr, "curl_url_set returned %d\n", (int)result);
  if(result == CURLUE_OK) {
    char *host = NULL;
    result = curl_url_get(url, CURLUPART_HOST, &host, 0);
    fprintf(stderr, "curl_url_get returned %d, host %s\n",
            (int)result, host ? host : "(null)");
    curl_free(host);
  }
  curl_url_cleanup(url);
  return result == CURLUE_OK ? 0 : 1;
}
