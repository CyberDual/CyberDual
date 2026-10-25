#define PCRE2_CODE_UNIT_WIDTH 8
#include "pcre2.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
  const char *pattern = "(?=(?=(?=(?=(?=(?=(?=(?=a))(?R))))))).";
  const char *subject = "a";
  int error;
  PCRE2_SIZE error_offset;
  int workspace[1024];
  pcre2_code *code = pcre2_compile((PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED,
    0, &error, &error_offset, NULL);
  if (code == NULL)
    {
    fprintf(stderr, "compile error %d at %zu\n", error, (size_t)error_offset);
    return 2;
    }

  pcre2_match_data *data = pcre2_match_data_create_from_pattern(code, NULL);
  pcre2_match_context *context = pcre2_match_context_create(NULL);
  if (data == NULL || context == NULL || pcre2_set_heap_limit(context, 4) != 0)
    return 2;

  int result = pcre2_dfa_match(code, (PCRE2_SPTR)subject, strlen(subject), 0,
    0, data, context, workspace, 1024);
  printf("match result: %d\n", result);

  pcre2_match_context_free(context);
  pcre2_match_data_free(data);
  pcre2_code_free(code);
  return 0;
}
