#include <stdio.h>

#define PCRE2_CODE_UNIT_WIDTH 8
#include "pcre2.h"

int main(void)
{
int errorcode;
PCRE2_SIZE erroroffset;
PCRE2_SIZE workspace[1000];
pcre2_code *code;
pcre2_match_data *match_data;

code = pcre2_compile(
  (PCRE2_SPTR)"(*LIMIT_HEAP=4)(?=(?=(?=(?=(?=(?=(?=(?=a))(?R))))))).",
  PCRE2_ZERO_TERMINATED, 0, &errorcode, &erroroffset, NULL);
if (code == NULL)
  {
  fprintf(stderr, "compile failed at offset %lu\n", (unsigned long)erroroffset);
  return 1;
  }

match_data = pcre2_match_data_create(4, NULL);
if (match_data == NULL)
  {
  pcre2_code_free(code);
  return 2;
  }

errorcode = pcre2_dfa_match(
  code, (PCRE2_SPTR)"a", 1, 0, 0, match_data, NULL, (int *)workspace,
  sizeof(workspace) / sizeof(workspace[0]));
printf("match result: %d\n", errorcode);

pcre2_match_data_free(match_data);
pcre2_code_free(code);
return 0;
}
