#include <curl/curl.h>

int main(void)
{
  char text[] = "x";
  char *out;

  /* GCC's format checker rejects a negative precision, which this API
     defines as "precision omitted". Keep the call on the public entry. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
#pragma GCC diagnostic ignored "-Wformat-extra-args"
  out = curl_maprintf("%#-2.-2s", text);
#pragma GCC diagnostic pop
  if(out)
    curl_free(out);
  return 0;
}
