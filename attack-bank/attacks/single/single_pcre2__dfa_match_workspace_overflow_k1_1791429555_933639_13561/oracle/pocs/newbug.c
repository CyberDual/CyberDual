#define PCRE2_CODE_UNIT_WIDTH 8
#include "pcre2.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
  enum { DEPTH = 20, ALTERNATIVES = 166 };
  char pattern[DEPTH * 4 + ALTERNATIVES * 2 + 2];
  char *out = pattern;
  int workspace[1000];
  int errorcode;
  PCRE2_SIZE erroroffset;
  pcre2_code *code;
  pcre2_match_data *data;
  pcre2_match_context *context;
  int rc;

  for (int i = 0; i < DEPTH; ++i) {
    memcpy(out, "(?=", 3);
    out += 3;
  }
  for (int i = 0; i < ALTERNATIVES; ++i) {
    *out++ = 'a';
    if (i + 1 < ALTERNATIVES) *out++ = '|';
  }
  for (int i = 0; i < DEPTH; ++i)
    *out++ = ')';
  *out = '\0';

  code = pcre2_compile((PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED, 0,
                       &errorcode, &erroroffset, NULL);
  if (code == NULL) {
    fprintf(stderr, "compile failed: %d at %zu\n", errorcode, (size_t)erroroffset);
    return 2;
  }

  data = pcre2_match_data_create_from_pattern(code, NULL);
  context = pcre2_match_context_create(NULL);
  if (data == NULL || context == NULL) return 2;
  pcre2_set_heap_limit(context, 51);
  rc = pcre2_dfa_match(code, (PCRE2_SPTR)"a", 1, 0, 0, data, context,
                       workspace, 1000);
  printf("match result: %d\n", rc);

  pcre2_match_context_free(context);
  pcre2_match_data_free(data);
  pcre2_code_free(code);
  return 0;
}
