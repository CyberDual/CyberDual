#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>

const char *__asan_default_options(void)
{
  return "abort_on_error=1";
}

int main(void)
{
  /* heap-allocate a short string so an over-read is caught by ASan */
  char *str = malloc(6);
  char *out;
  if(!str)
    return 2;
  memcpy(str, "hello", 6);

  /* "A negative precision is taken as if the precision were omitted." */
  out = curl_maprintf("%.*s", -2, str);
  if(out) {
    printf("[%s]\n", out);
    curl_free(out);
  }
  free(str);
  return 0;
}
