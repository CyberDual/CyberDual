#define PCRE2_CODE_UNIT_WIDTH 8
#include <stdio.h>
#include <stdlib.h>
#include "pcre2.h"

/* In-contract glob conversion: separator '.' must be escaped inside a
   negated class when wildcards do not match the separator. */
int main(void)
{
pcre2_convert_context *ctx;
PCRE2_UCHAR *converted = NULL;
PCRE2_SIZE outlen = 0;
int rc;
static const unsigned char pattern[] = { '[', '!', 'a', ']' };

ctx = pcre2_convert_context_create(NULL);
if (ctx == NULL)
  return 2;
if (pcre2_set_glob_separator(ctx, '.') != 0)
  return 3;

rc = pcre2_pattern_convert((PCRE2_SPTR)pattern, sizeof(pattern),
  PCRE2_CONVERT_GLOB, &converted, &outlen, ctx);

if (rc == 0 && converted != NULL)
  {
  fwrite(converted, 1, outlen, stdout);
  fputc('\n', stdout);
  pcre2_converted_pattern_free(converted);
  }
else
  fprintf(stderr, "convert failed: %d\n", rc);

pcre2_convert_context_free(ctx);
return rc == 0 ? 0 : 1;
}
