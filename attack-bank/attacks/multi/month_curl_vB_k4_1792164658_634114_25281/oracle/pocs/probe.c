#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
  char *s = malloc(4);
  char *out;

  if(!s)
    return 1;
  memcpy(s, "abc", 4);
  out = curl_maprintf("%s", s);
  free(out);
  free(s);
  return 0;
}
