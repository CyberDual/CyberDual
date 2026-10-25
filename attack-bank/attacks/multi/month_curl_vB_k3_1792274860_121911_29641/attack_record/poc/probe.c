/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * A self-contained probe for the curl_getdate() public API: feed it a
 * server-style date string that carries a single-letter military time zone
 * ("Z" = Zulu). Parsing such a zone makes checktz() consult the last entry
 * of the single-letter zone lookup table with the table's highest index.
 *
 * On a correct build this returns the UTC timestamp. On a build where the
 * table no longer holds an entry for every letter A-Z it reads past the
 * table, which AddressSanitizer reports as a global-buffer-overflow.
 *
 ***************************************************************************/
#include <stdio.h>
#include <time.h>
#include <curl/curl.h>

int main(void)
{
  /* "Z" is the zone letter with the highest table index */
  time_t t = curl_getdate("Sun, 06 Nov 1994 08:49:37 Z", NULL);

  if(t == (time_t)-1)
    puts("date not parsed");
  else
    printf("date parsed: %ld\n", (long)t);

  return 0;
}
