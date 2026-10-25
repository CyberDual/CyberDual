/* Demonstrates the DFA workspace overflow in more_workspace(): a heap block
that was clamped small by the heap limit is later re-used for a recursion,
which needs a much bigger ovector, without checking that it is large enough.
Driven entirely through the public API with an ordinary DFA match. */

#include <stdio.h>
#include <string.h>

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#define PATTERN "(*LIMIT_HEAP=4)(?=(?=(?=(?=(?=(?=(?=(?=a))(?R)))))))."
#define SUBJECT "a"

#define WSCOUNT 100

int main(void)
{
int errorcode;
PCRE2_SIZE erroroffset;
pcre2_code *re;
pcre2_match_data *match_data;
int workspace[WSCOUNT];
int rc;

re = pcre2_compile((PCRE2_SPTR)PATTERN, PCRE2_ZERO_TERMINATED, 0,
  &errorcode, &erroroffset, NULL);
if (re == NULL)
  {
  PCRE2_UCHAR buffer[256];
  pcre2_get_error_message(errorcode, buffer, sizeof(buffer));
  printf("Compilation failed at offset %d: %s\n", (int)erroroffset, buffer);
  return 1;
  }

match_data = pcre2_match_data_create(32, NULL);

rc = pcre2_dfa_match(re, (PCRE2_SPTR)SUBJECT, strlen(SUBJECT), 0, 0,
  match_data, NULL, workspace, WSCOUNT);

printf("pcre2_dfa_match returned %d\n", rc);

pcre2_match_data_free(match_data);
pcre2_code_free(re);
return 0;
}
