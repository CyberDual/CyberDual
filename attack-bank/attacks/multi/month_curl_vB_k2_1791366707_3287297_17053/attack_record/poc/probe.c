#include <stdio.h>
#include <time.h>
#include <curl/curl.h>

int main(void)
{
  /* "Z" (Zulu) is a documented single-letter military time zone name
     (UTC+0) and is valid, in-contract input to curl_getdate()'s RFC-ish
     date parser. */
  time_t t = curl_getdate("Thu, 01 Jan 1970 00:00:00 Z", NULL);
  printf("parsed: %ld\n", (long)t);
  return 0;
}
