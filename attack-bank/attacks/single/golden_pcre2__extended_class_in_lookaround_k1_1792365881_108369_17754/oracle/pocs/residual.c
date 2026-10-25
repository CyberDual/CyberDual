#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include <stdint.h>
#include <stdio.h>

int main(void)
{
  const char *pattern = "(?<=[1[[[[[2]]]]]]])";
  int error_code;
  PCRE2_SIZE error_offset;
  pcre2_code *code = pcre2_compile(
    (PCRE2_SPTR)(const uint8_t *)pattern, PCRE2_ZERO_TERMINATED,
    PCRE2_ALT_EXTENDED_CLASS, &error_code, &error_offset, NULL);

  if (code == NULL)
    {
    PCRE2_UCHAR message[256];
    pcre2_get_error_message(error_code, message, sizeof(message));
    fprintf(stderr, "pcre2_compile failed at offset %zu: %s\n",
      (size_t)error_offset, message);
    return 1;
    }

  pcre2_code_free(code);
  return 0;
}
