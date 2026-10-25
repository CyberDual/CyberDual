#define PCRE2_CODE_UNIT_WIDTH 8

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pcre2.h"

/* Variable-length lookbehind over an invalid-UTF subject whose checked
fragment is only one byte past the start of the buffer. */
int main(void)
{
  static const unsigned char bytes[] = { 0xff, 'a', 'X', 0xfe };
  unsigned char *subject;
  pcre2_code *re;
  pcre2_match_data *md;
  int err;
  int rc;
  PCRE2_SIZE erroff;

  subject = malloc(sizeof bytes);
  if (subject == NULL) return 2;
  memcpy(subject, bytes, sizeof bytes);

  re = pcre2_compile((PCRE2_SPTR)"(?<=a{1,3})X", PCRE2_ZERO_TERMINATED,
                     PCRE2_UTF | PCRE2_MATCH_INVALID_UTF, &err, &erroff, NULL);
  if (re == NULL)
    {
    free(subject);
    return 3;
    }

  md = pcre2_match_data_create_from_pattern(re, NULL);
  if (md == NULL)
    {
    pcre2_code_free(re);
    free(subject);
    return 4;
    }

  rc = pcre2_match(re, (PCRE2_SPTR)subject, sizeof bytes, 0, 0, md, NULL);
  printf("rc=%d\n", rc);

  pcre2_match_data_free(md);
  pcre2_code_free(re);
  free(subject);
  return 0;
}
