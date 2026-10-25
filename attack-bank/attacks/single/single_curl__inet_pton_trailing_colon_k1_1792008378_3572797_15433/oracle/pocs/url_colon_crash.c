/* Reach the IPv6 literal parser of the URL API with an address that has a
   '::' and one group too many. */
#include <stdio.h>
#include <curl/curl.h>

int main(void)
{
  CURLU *h = curl_url();
  if(h) {
    CURLUcode rc = curl_url_set(h, CURLUPART_URL,
                                "http://[1:2:3:4::5:6:7:8:9]/", 0);
    printf("curl_url_set returned %d\n", (int)rc);
    curl_url_cleanup(h);
  }
  return 0;
}
