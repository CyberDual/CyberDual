/* Reproducer: pcre2_match() with a lazy \X repeat runs the extended grapheme
cluster scan past the end of the subject when PCRE2_MATCH_INVALID_UTF is in
force and the final fragment of the subject ends with a truncated UTF
character. The subject buffer is allocated to its exact length so that the
out-of-bounds read is directly beyond the end of the block. */

#include <stdio.h>
#include <stdlib.h>

#define PCRE2_CODE_UNIT_WIDTH 8
#include "pcre2.h"

int main(void)
{
int errorcode;
PCRE2_SIZE erroroffset;
pcre2_code *re;
pcre2_match_data *md;
uint8_t *subject;
int rc;

re = pcre2_compile((PCRE2_SPTR)"\\X*?b", PCRE2_ZERO_TERMINATED,
  PCRE2_UTF|PCRE2_MATCH_INVALID_UTF|PCRE2_NO_START_OPTIMIZE, &errorcode,
  &erroroffset, NULL);
if (re == NULL)
  {
  PCRE2_UCHAR message[256];
  pcre2_get_error_message(errorcode, message, sizeof(message));
  printf("compile failed at offset %d: %s\n", (int)erroroffset, message);
  return 1;
  }

md = pcre2_match_data_create_from_pattern(re, NULL);

/* Valid 'a' followed by a 2-byte UTF-8 lead unit with no continuation, so
matching stops before the 0xcc but the cluster scan still reaches it. */

subject = (uint8_t *)malloc(2);
subject[0] = 'a';
subject[1] = 0xcc;

rc = pcre2_match(re, subject, 2, 0, 0, md, NULL);
printf("rc=%d\n", rc);

free(subject);
pcre2_match_data_free(md);
pcre2_code_free(re);
return 0;
}
