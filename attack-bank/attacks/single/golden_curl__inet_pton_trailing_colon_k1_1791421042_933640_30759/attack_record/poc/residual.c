#include <curl/curl.h>
#include <stdio.h>

int main(void)
{
  CURLU *url = curl_url();
  CURLUcode rc;
  if(!url)
    return 2;
  rc = curl_url_set(url, CURLUPART_URL,
                    "http://[1:2:3:4:5:6:7:8:9:a]/", 0);
  printf("curl_url_set: %d\n", (int)rc);
  curl_url_cleanup(url);
  return 0;
}
