#define PCRE2_CODE_UNIT_WIDTH 8
#include "pcre2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
  static const unsigned char bytes[] = {
    0x80, 0xf0, 0x9f, 0x87, 0xa6, 0xf0, 0x9f, 0x87, 0xa7
  };
  unsigned char *subject = malloc(sizeof(bytes));
  pcre2_code *code;
  pcre2_match_data *data;
  PCRE2_SIZE error_offset;
  int error_code;
  int result;

  if (subject == NULL) return 2;
  memcpy(subject, bytes, sizeof(bytes));
  code = pcre2_compile((PCRE2_SPTR)"\\X{2}", PCRE2_ZERO_TERMINATED,
    PCRE2_MATCH_INVALID_UTF, &error_code, &error_offset, NULL);
  if (code == NULL) return 2;
  data = pcre2_match_data_create_from_pattern(code, NULL);
  if (data == NULL) return 2;

  result = pcre2_match(code, subject, sizeof(bytes), 0, 0, data, NULL);
  printf("match result: %d\n", result);

  pcre2_match_data_free(data);
  pcre2_code_free(code);
  free(subject);
  return result == PCRE2_ERROR_NOMATCH ? 0 : 1;
}
