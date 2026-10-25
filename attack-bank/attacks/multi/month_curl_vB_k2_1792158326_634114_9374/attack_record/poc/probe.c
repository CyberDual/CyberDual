#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

int main(void)
{
  char *buf = malloc(4);
  char *out;

  if(!buf)
    return 1;
  /* Precision covers the object exactly. No terminator is required. */
  memcpy(buf, "abcd", 4);
  out = curl_maprintf("%.4s", buf);
  curl_free(out);
  free(buf);
  return 0;
}
