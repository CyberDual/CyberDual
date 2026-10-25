#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <string.h>
int main(void) {
  unsigned char pat[206];
  int err; PCRE2_SIZE erroff;
  memset(pat, 'a', 204); pat[204] = ' '; pat[205] = 0;
  pcre2_code *re = pcre2_compile(pat, PCRE2_ZERO_TERMINATED,
      PCRE2_EXTENDED | PCRE2_AUTO_CALLOUT, &err, &erroff, NULL);
  if (re) pcre2_code_free(re);
  return 0;
}
