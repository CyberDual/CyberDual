#include <curl/curl.h>
#include <curl/mprintf.h>
#include <stdlib.h>

#define FMT16 "%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c"
#define ARG16 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', \
              'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a'

int main(void)
{
  char *result = curl_maprintf(FMT16 FMT16 FMT16 FMT16
                               FMT16 FMT16 FMT16 FMT16 "%c",
                               ARG16, ARG16, ARG16, ARG16,
                               ARG16, ARG16, ARG16, ARG16, 'z');
  free(result);
  return 0;
}
