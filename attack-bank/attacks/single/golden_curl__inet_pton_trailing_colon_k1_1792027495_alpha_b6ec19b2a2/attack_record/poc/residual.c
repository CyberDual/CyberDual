#include <stdio.h>
#include <curl/curl.h>

int main(void)
{
  CURLU *u;
  CURLUcode rc;

  u = curl_url();
  if(!u)
    return 2;
  rc = curl_url_set(u, CURLUPART_URL, "http://[1:2:3:4:5:6:7:8:9:a]/", 0);
  curl_url_cleanup(u);
  return (rc == CURLUE_BAD_IPV6) ? 0 : 1;
}
