/* Drives libcurl's public mprintf API with an attacker-controlled format
 * string. curl's printf substitutes the constant "(nil)" for a NULL string
 * argument, so passing NULL with an explicit precision is in-contract input
 * for this API. */
#include <stdio.h>
#include <string.h>
#include <curl/curl.h>

/* Ask the sanitizer to abort (SIGABRT) when it reports, unless the
   environment overrides it. */
const char *__asan_default_options(void)
{
  return "abort_on_error=1";
}

int main(void)
{
  /* control conversions: ordinary, always-safe input */
  char *c1 = curl_maprintf("%s", "ordinary");
  char *c2 = curl_maprintf("%.3s", "truncated");

  /* NULL string argument with an explicit precision */
  char *s = curl_maprintf("%.500s", (const char *)NULL);

  size_t n1 = c1 ? strlen(c1) : 0;
  size_t n2 = c2 ? strlen(c2) : 0;
  size_t n3 = s ? strlen(s) : 0;
  printf("%zu %zu %zu\n", n1, n2, n3);

  curl_free(c1);
  curl_free(c2);
  curl_free(s);
  return 0;
}
