#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define PCRE2_CODE_UNIT_WIDTH 8
#include "pcre2.h"

int main(void)
{
  const char *pattern = "(?<=a{1,2})X";
  uint8_t subject[] = { 0x80, 'X' };
  int errorcode;
  PCRE2_SIZE erroroffset;
  uint8_t *heap_subject = malloc(sizeof(subject));
  pcre2_match_data *match_data;

  pcre2_code *code = pcre2_compile(
    (PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED,
    PCRE2_UTF | PCRE2_MATCH_INVALID_UTF, &errorcode, &erroroffset, NULL);
  if (code == NULL)
    {
    fprintf(stderr, "pattern compilation failed at offset %lu\n",
      (unsigned long)erroroffset);
    return 1;
    }

  if (heap_subject == NULL)
    {
    pcre2_code_free(code);
    return 1;
    }

  heap_subject[0] = subject[0];
  heap_subject[1] = subject[1];
  match_data = pcre2_match_data_create_from_pattern(code, NULL);
  if (match_data == NULL)
    {
    pcre2_code_free(code);
    return 1;
    }

  pcre2_match(code, heap_subject, sizeof(subject), 0, 0, match_data, NULL);
  free(heap_subject);
  pcre2_match_data_free(match_data);
  pcre2_code_free(code);
  return 0;
}
