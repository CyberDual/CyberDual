#include <curl/curl.h>
#include <stdlib.h>

int main(void)
{
  char format[3 * 129 + 1];
  char *result;
  size_t i;

  for(i = 0; i < 129; i++) {
    format[3 * i] = 'x';
    format[3 * i + 1] = '%';
    format[3 * i + 2] = '%';
  }
  format[sizeof(format) - 1] = '\0';

  result = curl_maprintf(format);
  curl_free(result);
  return 0;
}
