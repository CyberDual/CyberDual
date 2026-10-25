#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
  enum { N = 16 };
  char *buf = malloc(N);
  char *out;

  if(!buf)
    return 0;
  memset(buf, 'A', N);
  out = curl_maprintf("%.*s", N, buf);
  curl_free(out);
  free(buf);
  return 0;
}
