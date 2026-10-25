#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <string.h>

int main(void)
{
char pattern[64];
char *p = pattern;
int errorcode;
PCRE2_SIZE erroroffset;
pcre2_code *code;
pcre2_match_data *match_data;
int i;

memcpy(p, "(?<=", 4);
p += 4;
for (i = 0; i < 15; i++) *p++ = '[';
*p++ = 'a';
for (i = 0; i < 15; i++) *p++ = ']';
*p++ = ')';
*p++ = 'b';
*p = 0;

code = pcre2_compile((PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED,
  PCRE2_ALT_EXTENDED_CLASS, &errorcode, &erroroffset, NULL);
if (code == NULL) return 0;

match_data = pcre2_match_data_create_from_pattern(code, NULL);
if (match_data == NULL)
  {
  pcre2_code_free(code);
  return 1;
  }

i = pcre2_match(code, (PCRE2_SPTR)"ab", 2, 0, 0, match_data, NULL);
pcre2_match_data_free(match_data);
pcre2_code_free(code);
return i < 0;
}
