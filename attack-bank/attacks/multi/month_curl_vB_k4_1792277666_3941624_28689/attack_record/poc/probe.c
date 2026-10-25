#include <curl/curl.h>
#include <stddef.h>
#include <string.h>

int main(void)
{
  static const char conversion[] = "%1$d";
  char format[sizeof(conversion) * 128];
  char *output;
  size_t offset = 0;
  size_t i;

  for(i = 0; i < 128; i++) {
    memcpy(format + offset, conversion, sizeof(conversion) - 1);
    offset += sizeof(conversion) - 1;
  }
  format[offset] = '\0';

  output = curl_maprintf(format, 0);
  curl_free(output);
  return 0;
}
