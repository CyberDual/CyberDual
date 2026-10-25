/* Extract string 0 from a hard partial match. The first ovector pair after a
partial match describes the matched portion of the subject, and extracting
string 0 from it is supported behaviour. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PCRE2_CODE_UNIT_WIDTH 8
#include "pcre2.h"

int main(void)
{
  int errorcode, rc;
  PCRE2_SIZE erroffset, length, subject_length;
  pcre2_code *code;
  pcre2_match_data *match_data;
  PCRE2_UCHAR *substring;
  PCRE2_SPTR subject;
  char *buffer;

  code = pcre2_compile((PCRE2_SPTR)"A+Q", PCRE2_ZERO_TERMINATED, 0,
    &errorcode, &erroffset, NULL);
  if (code == NULL)
  {
    printf("pcre2_compile() failed: %d at offset %lu\n", errorcode,
      (unsigned long)erroffset);
    return 1;
  }

  /* A subject that ends part-way through a potential match: 200 filler
  characters followed by 20 characters that begin a match of the pattern. */

  subject_length = 220;
  buffer = (char *)malloc(subject_length);
  if (buffer == NULL) return 2;
  memset(buffer, 'Z', 200);
  memset(buffer + 200, 'A', 20);
  subject = (PCRE2_SPTR)buffer;

  match_data = pcre2_match_data_create_from_pattern(code, NULL);
  if (match_data == NULL) return 3;

  rc = pcre2_match(code, subject, subject_length, 0, PCRE2_PARTIAL_HARD,
    match_data, NULL);
  printf("pcre2_match() returned %d\n", rc);
  if (rc != PCRE2_ERROR_PARTIAL)
  {
    printf("expected a partial match\n");
    return 4;
  }

  {
    PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match_data);
    printf("partial match ovector: (%lu, %lu)\n",
      (unsigned long)ovector[0], (unsigned long)ovector[1]);
  }

  rc = pcre2_substring_get_bynumber(match_data, 0, &substring, &length);
  printf("pcre2_substring_get_bynumber() returned %d\n", rc);
  if (rc == 0)
  {
    printf("extracted %lu code units\n", (unsigned long)length);
    pcre2_substring_free(substring);
  }

  pcre2_match_data_free(match_data);
  pcre2_code_free(code);
  free(buffer);
  return 0;
}
