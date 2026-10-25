#define PCRE2_CODE_UNIT_WIDTH 8
#include "pcre2.h"

int main(void)
{
  int error;
  PCRE2_SIZE error_offset;
  PCRE2_UCHAR output[8];
  PCRE2_SIZE output_length = sizeof(output);
  const PCRE2_UCHAR subject[] = "";
  const PCRE2_UCHAR replacement[] = "x";
  pcre2_code *code = pcre2_compile((PCRE2_SPTR)"^$", PCRE2_ZERO_TERMINATED, 0,
                                   &error, &error_offset, NULL);
  if (code == NULL)
    return 1;

  pcre2_match_data *data = pcre2_match_data_create_from_pattern(code, NULL);
  if (data == NULL)
    return 2;

  if (pcre2_match(code, subject, 0, 0, PCRE2_COPY_MATCHED_SUBJECT, data, NULL) < 0)
    return 3;

  int rc = pcre2_substitute(code, subject, 0, 0,
                            PCRE2_COPY_MATCHED_SUBJECT | PCRE2_SUBSTITUTE_MATCHED,
                            data, NULL, replacement, 1, output, &output_length);
  pcre2_match_data_free(data);
  pcre2_code_free(code);
  return rc < 0 ? 4 : 0;
}
