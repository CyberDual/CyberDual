/*
Compile a large pattern that exercises the extended-class length scan inside
a lookbehind. The bulk of the pattern is a comment, so the compiled code is
tiny; only the parse of the lookbehind matters. Build with AddressSanitizer:

  cc -fsanitize=address -g -O1 -I<builddir>/interface newbug.c \
     <builddir>/libpcre2-8.a -lpthread

Run with: ASAN_OPTIONS=abort_on_error=1:detect_leaks=0 ./newbug
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define PCRE2_CODE_UNIT_WIDTH 8
#include "pcre2.h"

#define PAD 200000

int main(void)
{
int errcode;
PCRE2_SIZE erroff;
pcre2_code *re;
const char *tail = "(?<=[x[]]])";
size_t tailen = strlen(tail);
PCRE2_UCHAR *buffer = malloc((PAD + tailen + 5) * sizeof(PCRE2_UCHAR));
if (buffer == NULL)
  {
  fprintf(stderr, "out of memory\n");
  return 1;
  }

buffer[0] = '(';
buffer[1] = '?';
buffer[2] = '#';
memset(buffer + 3, 'a', PAD);
buffer[3 + PAD] = ')';
memcpy(buffer + 4 + PAD, tail, tailen);

re = pcre2_compile(buffer, PAD + tailen + 4,
  PCRE2_ALT_EXTENDED_CLASS | PCRE2_ALLOW_EMPTY_CLASS, &errcode, &erroff, NULL);
printf("result: %s err=%d\n", re != NULL ? "OK" : "FAIL", errcode);

if (re != NULL) pcre2_code_free(re);
free(buffer);
return 0;
}
