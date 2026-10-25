#include <curl/curl.h>

int main(void)
{
  (void)curl_getdate("Funday, 06 Nov 1994 08:49:37 GMT", NULL);
  return 0;
}
