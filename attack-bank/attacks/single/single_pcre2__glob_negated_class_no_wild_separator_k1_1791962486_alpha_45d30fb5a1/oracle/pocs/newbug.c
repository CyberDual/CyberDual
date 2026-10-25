#define PCRE2_CODE_UNIT_WIDTH 8

#include <stdio.h>
#include <string.h>

#include "pcre2.h"

/* Glob whose negated class lists eight ordinary literals.
   PCRE2_CONVERT_GLOB_NO_WILD_SEPARATOR lets wildcards match the separator,
   so the class must not have the separator forced into it. */
int main(void)
{
  static const char pattern[] = "[!abcdefgh]";
  PCRE2_UCHAR *converted = NULL;
  PCRE2_SIZE outlen = 0;
  uint32_t options = PCRE2_CONVERT_GLOB | PCRE2_CONVERT_GLOB_NO_WILD_SEPARATOR;
  int rc;

  rc = pcre2_pattern_convert((PCRE2_SPTR)pattern, strlen(pattern), options,
                             &converted, &outlen, NULL);
  if (rc == 0 && converted != NULL)
    printf("%s\n", (char *)converted);
  else
    fprintf(stderr, "convert failed: %d\n", rc);
  pcre2_converted_pattern_free(converted);
  return rc == 0 ? 0 : 1;
}
