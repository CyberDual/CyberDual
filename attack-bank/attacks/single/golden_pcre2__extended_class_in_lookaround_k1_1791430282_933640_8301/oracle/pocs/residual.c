#define PCRE2_CODE_UNIT_WIDTH 8
#include "pcre2.h"
#include <stdio.h>

int main(void)
{
  static const unsigned char pattern[] = "(?<=[\\p{L}&&[^\\p{Greek}]])";
  int error;
  PCRE2_SIZE offset;
  pcre2_code *code = pcre2_compile(pattern, sizeof(pattern) - 1,
    PCRE2_UTF | PCRE2_UCP | PCRE2_ALT_EXTENDED_CLASS,
    &error, &offset, NULL);

  if (code == NULL)
    {
    fprintf(stderr, "compile error %d at %zu\n", error, (size_t)offset);
    return 1;
    }

  pcre2_code_free(code);
  return 0;
}
