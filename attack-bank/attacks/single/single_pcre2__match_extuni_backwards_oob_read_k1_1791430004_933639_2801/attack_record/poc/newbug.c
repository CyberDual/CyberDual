#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <stdlib.h>

int main(void)
{
  static const PCRE2_UCHAR pattern[] = "\\X*[^A]";
  PCRE2_UCHAR *subject = malloc(1);
  pcre2_code *code;
  pcre2_match_data *data;
  int error;
  PCRE2_SIZE offset;

  if (subject == NULL) return 1;
  subject[0] = 'A';
  code = pcre2_compile(pattern, PCRE2_ZERO_TERMINATED, PCRE2_UTF | PCRE2_UCP,
    &error, &offset, NULL);
  if (code == NULL) return 2;
  data = pcre2_match_data_create_from_pattern(code, NULL);
  if (data == NULL) return 3;
  (void)pcre2_match(code, subject, 1, 0, PCRE2_ANCHORED, data, NULL);
  pcre2_match_data_free(data);
  pcre2_code_free(code);
  free(subject);
  return 0;
}
