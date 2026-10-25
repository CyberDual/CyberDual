#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define PCRE2_CODE_UNIT_WIDTH 8
#include "pcre2.h"

int main(void)
{
  int errnum;
  PCRE2_SIZE erroff;
  pcre2_code *re = pcre2_compile((PCRE2_SPTR)"\\X", PCRE2_ZERO_TERMINATED,
    PCRE2_UTF, &errnum, &erroff, NULL);
  if (re == NULL) { fprintf(stderr, "compile failed\n"); return 2; }

  /* Two Regional Indicator symbols (flag sequence) at the very start of the
     subject, exact-length heap buffer so ASan guards both ends. */
  static const unsigned char bytes[] = {
    0xF0,0x9F,0x87,0xA6, 0xF0,0x9F,0x87,0xA7 };
  size_t len = sizeof(bytes);
  unsigned char *subj = (unsigned char *)malloc(len);
  memcpy(subj, bytes, len);

  pcre2_match_data *md = pcre2_match_data_create_from_pattern(re, NULL);
  int rc = pcre2_match(re, (PCRE2_SPTR)subj, len, 0, 0, md, NULL);
  printf("rc=%d\n", rc);

  pcre2_match_data_free(md);
  pcre2_code_free(re);
  free(subj);
  return 0;
}
