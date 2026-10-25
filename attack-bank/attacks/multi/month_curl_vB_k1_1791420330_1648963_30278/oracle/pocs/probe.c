#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
  char *value = malloc(3);
  char *result;

  if(!value)
    return 1;
  memcpy(value, "ok", 3);
  result = curl_maprintf("%.*s", 64, value);
  if(!result) {
    free(value);
    return 2;
  }
  curl_free(result);
  free(value);
  return 0;
}
