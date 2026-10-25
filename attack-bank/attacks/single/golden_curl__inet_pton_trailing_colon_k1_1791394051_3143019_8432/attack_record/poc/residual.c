/* PoC: drive curlx_inet_pton(AF_INET6, ...) through the public URL API.
 *
 * curl_url_set(CURLUPART_URL, ...) parses the bracketed IPv6 host and hands
 * the textual address to curlx_inet_pton(). An over-long IPv6 literal
 * ("1:2:3:4:5:6:7:8:9:a" -- ten 16-bit groups) must be rejected. On a tree
 * that bounds the per-group store correctly this returns CURLUE_BAD_IPV6.
 *
 * Build (static libcurl built with -fsanitize=address):
 *   cc -fsanitize=address -I<srcroot>/include residual.c \
 *      <builddir>/lib/libcurl-d.a -lpthread -o residual
 * Run:
 *   ./residual
 */
#include <stdio.h>
#include <curl/curl.h>

int main(void)
{
  CURLU *h = curl_url();
  CURLUcode rc;

  if(!h)
    return 2;

  rc = curl_url_set(h, CURLUPART_URL,
                    "http://[1:2:3:4:5:6:7:8:9:a]/", 0);

  printf("curl_url_set rc=%d\n", (int)rc);

  curl_url_cleanup(h);
  return 0;
}
