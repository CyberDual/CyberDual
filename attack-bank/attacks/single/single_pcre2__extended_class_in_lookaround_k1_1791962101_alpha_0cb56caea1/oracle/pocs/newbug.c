#define PCRE2_CODE_UNIT_WIDTH 8
#include <stdio.h>
#include <string.h>
#include "pcre2.h"

/* Maximum nested extended class the parser accepts (ECLASS_NEST_LIMIT),
inside a fixed-length lookbehind. */
int main(void)
{
char pat[64];
char *p = pat;
int n = 15;
int err;
PCRE2_SIZE off;
pcre2_code *re;

memcpy(p, "(?<=", 4);
p += 4;
for (int i = 0; i < n; i++) *p++ = '[';
*p++ = 'a';
for (int i = 0; i < n; i++) *p++ = ']';
*p++ = ')';
*p = 0;

re = pcre2_compile((PCRE2_SPTR)pat, PCRE2_ZERO_TERMINATED,
  PCRE2_ALT_EXTENDED_CLASS, &err, &off, NULL);
if (re != NULL) pcre2_code_free(re);
printf("err=%d\n", err);
return 0;
}
