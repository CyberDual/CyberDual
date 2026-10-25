#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
int errorcode = 0;
PCRE2_SIZE erroroffset = 0;
pcre2_code *code = pcre2_compile((PCRE2_SPTR)"\\X{2}", PCRE2_ZERO_TERMINATED,
  PCRE2_UTF, &errorcode, &erroroffset, NULL);
if (code == NULL) return 2;

pcre2_match_data *match = pcre2_match_data_create_from_pattern(code, NULL);
uint8_t *subject = malloc(8);
if (subject == NULL) return 3;

subject[0] = 0xf0; subject[1] = 0x9f; subject[2] = 0x87; subject[3] = 0xa6;
subject[4] = 0xf0; subject[5] = 0x9f; subject[6] = 0x87; subject[7] = 0xa6;

int rc = pcre2_match(code, subject, 8, 0, 0, match, NULL);
printf("rc=%d\n", rc);

free(subject);
pcre2_match_data_free(match);
pcre2_code_free(code);
return rc < 0 && rc != PCRE2_ERROR_NOMATCH;
}
