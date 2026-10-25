#include <string.h>
#define PCRE2_CODE_UNIT_WIDTH 8
#include "pcre2.h"

int main(void)
{
int errcode;
PCRE2_SIZE erroff;
pcre2_code *re;
pcre2_match_context *mcontext;
pcre2_match_data *match_data;
int workspace[1000];
static const char pattern[] = "^((.)(?1)|.)$";
static const char subject[] = "aaa";
int rc;

re = pcre2_compile((PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED, 0,
  &errcode, &erroff, NULL);
if (re == NULL) return 2;

mcontext = pcre2_match_context_create(NULL);
if (mcontext == NULL) return 2;
pcre2_set_heap_limit(mcontext, 12);

match_data = pcre2_match_data_create(16, NULL);
if (match_data == NULL) return 2;

rc = pcre2_dfa_match(re, (PCRE2_SPTR)subject, (PCRE2_SIZE)strlen(subject),
  0, 0, match_data, mcontext, workspace, 1000);

pcre2_match_data_free(match_data);
pcre2_match_context_free(mcontext);
pcre2_code_free(re);
return rc < 0 ? 1 : 0;
}
