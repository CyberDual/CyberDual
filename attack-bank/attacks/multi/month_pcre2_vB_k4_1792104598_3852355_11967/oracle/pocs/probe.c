#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

int main(void)
{
  static const char *patterns[] = {
      "[acegi]",
      "[acegi]+",
      "(?:[acegi])",
      "[acegi][acegi]",
  };

  for (unsigned int i = 0; i < sizeof(patterns) / sizeof(patterns[0]); i++)
  {
    int error;
    PCRE2_SIZE offset;
    pcre2_code *code = pcre2_compile((PCRE2_SPTR)patterns[i], PCRE2_ZERO_TERMINATED, 0,
                                     &error, &offset, NULL);
    if (code == NULL)
      return 1;

    int rc = pcre2_jit_compile(code, PCRE2_JIT_COMPLETE);
    pcre2_code_free(code);
    if (rc != 0)
      return 1;
  }

  return 0;
}
