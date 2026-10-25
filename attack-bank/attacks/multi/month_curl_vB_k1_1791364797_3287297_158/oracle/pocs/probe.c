#include <curl/curl.h>
#include <stdio.h>

int main(void)
{
  /* RFC 822 permits an obsolete single-letter ("military") time zone
   * designator at the end of a date string. Feed one to the public
   * date parser with otherwise ordinary, in-contract input. */
  time_t t = curl_getdate("Sun, 06 Nov 1994 08:49:37 Z", NULL);
  printf("parsed: %ld\n", (long)t);
  return 0;
}
