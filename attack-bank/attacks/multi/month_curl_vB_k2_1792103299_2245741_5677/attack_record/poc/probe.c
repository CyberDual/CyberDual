#include <stdio.h>
#include <curl/curl.h>

/* Drives curl's public printf family with an attacker-controlled format
   string: a literal field width that does not fit an int. */
int main(void)
{
  const char *fmt = "%4294967295s";
  char *out = curl_maprintf(fmt, "hello");
  if(out) {
    fprintf(stderr, "result: '%s'\n", out);
    curl_free(out);
  }
  else {
    fprintf(stderr, "curl_maprintf returned NULL\n");
  }
  return 0;
}
