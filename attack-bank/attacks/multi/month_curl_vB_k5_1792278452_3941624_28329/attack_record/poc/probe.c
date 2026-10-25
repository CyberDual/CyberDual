#include <curl/curl.h>

#include <stdio.h>

int main(void)
{
  time_t parsed = curl_getdate("Thu, 01 Jan 1970 00:00:00 W", NULL);
  printf("%lld\n", (long long)parsed);
  return 0;
}
