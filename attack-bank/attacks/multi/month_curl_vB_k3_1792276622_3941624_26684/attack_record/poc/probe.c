#include <curl/curl.h>

int main(void)
{
  time_t parsed = curl_getdate("06 Nov 1994 08:49:37 Z", NULL);
  return parsed == -1 ? 1 : 0;
}
