#include <curl/curl.h>

/* Exercise curl_maprintf() with a documented format: ordinary flags, a
   decimal field width and a decimal precision on a floating point
   conversion. The width and precision digits are large but perfectly
   legal conversion specifications. */
int main(void)
{
  char *out = curl_maprintf("%+ -#2000000000.2000000000lf", 1.5);
  curl_mprintf("maprintf returned %s\n", out ? "string" : "NULL");
  curl_free(out);
  return 0;
}
