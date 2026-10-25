#include <curl/curl.h>
#include <stdio.h>

int main(void)
{
  CURLU *url = curl_url();
  char *host = NULL;

  if(!url)
    return 2;
  if(curl_url_set(url, CURLUPART_HOST, "%30", 0))
    return 3;
  if(curl_url_get(url, CURLUPART_HOST, &host, 0))
    return 4;
  puts(host);
  curl_free(host);
  curl_url_cleanup(url);
  return 0;
}
