#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include <stddef.h>

int main(void)
{
  int error_code;
  PCRE2_SIZE error_offset;
  PCRE2_SIZE subject_length;
  PCRE2_SIZE output_length = 32;
  PCRE2_SPTR owned_subject;
  PCRE2_UCHAR output[32];
  pcre2_code *code;
  pcre2_match_data *match_data;

  code = pcre2_compile((PCRE2_SPTR)"b", PCRE2_ZERO_TERMINATED, 0,
                       &error_code, &error_offset, NULL);
  if (code == NULL)
    return 2;

  match_data = pcre2_match_data_create_from_pattern(code, NULL);
  if (match_data == NULL)
    {
      pcre2_code_free(code);
      return 3;
    }

  if (pcre2_match(code, (PCRE2_SPTR)"abc", 3, 0,
                  PCRE2_COPY_MATCHED_SUBJECT, match_data, NULL) < 0)
    {
      pcre2_match_data_free(match_data);
      pcre2_code_free(code);
      return 4;
    }

  owned_subject = pcre2_get_subject(match_data, &subject_length);
  if (owned_subject == NULL || subject_length != 3)
    {
      pcre2_match_data_free(match_data);
      pcre2_code_free(code);
      return 5;
    }

  (void)pcre2_substitute(code, owned_subject, subject_length, 0,
                         PCRE2_COPY_MATCHED_SUBJECT | PCRE2_SUBSTITUTE_MATCHED,
                         match_data, NULL, (PCRE2_SPTR)"B", 1,
                         output, &output_length);

  pcre2_match_data_free(match_data);
  pcre2_code_free(code);
  return 0;
}
