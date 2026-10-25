#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

int main(void) {
  int errorcode = 0;
  PCRE2_SIZE erroroffset = 0;
  pcre2_code *code = pcre2_compile((PCRE2_SPTR)"a", PCRE2_ZERO_TERMINATED, 0,
                                   &errorcode, &erroroffset, NULL);
  if (code == NULL)
    return 0;

  pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(code, NULL);
  if (match_data != NULL) {
    (void)pcre2_match(code, (PCRE2_SPTR)"a", 1, 0, PCRE2_COPY_MATCHED_SUBJECT,
                      match_data, NULL);
    pcre2_match_data_free(match_data);
  }

  pcre2_code_free(code);
  return 0;
}
