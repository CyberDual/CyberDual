#include <curl/curl.h>

int main(void)
{
  char data[] = "abc";
  char *output = curl_maprintf("%.8s", data);

  curl_free(output);
  return 0;
}
