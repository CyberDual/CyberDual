#include <curl/curl.h>

int main(void)
{
  char *p = curl_maprintf("%.250d", 1);

  curl_free(p);
  return 0;
}
