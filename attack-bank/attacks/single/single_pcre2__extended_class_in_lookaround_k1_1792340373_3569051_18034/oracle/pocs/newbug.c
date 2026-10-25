#define PCRE2_CODE_UNIT_WIDTH 8
#include <stdio.h>
#include <string.h>
#include "pcre2.h"

int main(void)
{
  const char *pattern = "(?<=[[[[[[[[[[[[[[[a]]]]]]]]]]]]]]])";
  int errorcode = 0;
  PCRE2_SIZE erroroffset = 0;
  pcre2_code *code = pcre2_compile(
      (PCRE2_SPTR)pattern,
      strlen(pattern),
      PCRE2_ALT_EXTENDED_CLASS,
      &errorcode,
      &erroroffset,
      NULL);
  if (code == NULL)
    {
    printf("compile failed: %d at %lu\n", errorcode, (unsigned long)erroroffset);
    return 0;
    }
  pcre2_code_free(code);
  return 0;
}
