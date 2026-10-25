#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

/* Deep DFA lookaheads plus a recursion, under a tight heap limit. */
int main(void)
{
pcre2_code *re;
pcre2_match_data *md;
PCRE2_SIZE erroffset;
int errcode;
int workspace[2048];
static const PCRE2_SPTR pattern =
  (PCRE2_SPTR)"(*LIMIT_HEAP=4)(?=(?=(?=(?=(?=(?=(?=(?=a))(?R))))))).";
static const PCRE2_SPTR subject = (PCRE2_SPTR)"a";

re = pcre2_compile(pattern, PCRE2_ZERO_TERMINATED, 0, &errcode, &erroffset,
  NULL);
if (re == NULL) return 2;

md = pcre2_match_data_create(16, NULL);
if (md == NULL) return 3;

(void)pcre2_dfa_match(re, subject, 1, 0, 0, md, NULL, workspace, 2048);
pcre2_match_data_free(md);
pcre2_code_free(re);
return 0;
}
