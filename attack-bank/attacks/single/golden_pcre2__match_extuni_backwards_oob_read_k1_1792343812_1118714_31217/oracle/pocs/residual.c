/* Regression-style reproducer: match a variable-length lookbehind pattern
   against a subject whose first byte is an isolated UTF-8 continuation
   byte, using PCRE2_MATCH_INVALID_UTF. On a correct library this simply
   yields "no match". */
#include <stdio.h>
#include <stdlib.h>

#define PCRE2_CODE_UNIT_WIDTH 8
#include "pcre2.h"

int main(void)
{
pcre2_code *re;
pcre2_match_data *match_data;
PCRE2_SPTR pattern = (PCRE2_SPTR)"(?<=a{1,2})X";
int errorcode;
PCRE2_SIZE erroffset;
int rc;

/* Allocate a tight heap buffer so any read before the subject start is
   caught by the sanitizer. */
PCRE2_UCHAR *subject = (PCRE2_UCHAR *)malloc(2);
if (subject == NULL) return 2;
subject[0] = 0x80;   /* isolated continuation byte: invalid UTF-8 */
subject[1] = 'X';

re = pcre2_compile(pattern, PCRE2_ZERO_TERMINATED,
  PCRE2_UTF | PCRE2_MATCH_INVALID_UTF, &errorcode, &erroffset, NULL);
if (re == NULL)
  {
  fprintf(stderr, "compile failed at offset %lu: error %d\n",
    (unsigned long)erroffset, errorcode);
  return 2;
  }

match_data = pcre2_match_data_create_from_pattern(re, NULL);
if (match_data == NULL) return 2;

rc = pcre2_match(re, subject, 2, 0, 0, match_data, NULL);
printf("match rc = %d (%s)\n", rc,
  (rc == PCRE2_ERROR_NOMATCH)? "no match" : "other");

pcre2_match_data_free(match_data);
pcre2_code_free(re);
free(subject);
return 0;
}
