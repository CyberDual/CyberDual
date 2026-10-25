#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

int main(void)
{
  int errorcode;
  PCRE2_SIZE erroffset;
  pcre2_code *code = pcre2_compile((PCRE2_SPTR)"[acegi]",
                                   PCRE2_ZERO_TERMINATED, 0, &errorcode, &erroffset, NULL);
  int result;

  if (code == NULL)
    return 2;

  result = pcre2_jit_compile(code, PCRE2_JIT_COMPLETE);
  pcre2_code_free(code);
  return result == 0 ? 0 : 3;
}
