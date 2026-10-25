#include <stdio.h>
#include <string.h>
#define PCRE2_CODE_UNIT_WIDTH 8
#include "pcre2.h"

int main(void)
{
const char *pattern = "(a(?1)?b)";
const char *subject = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaab";
int errcode;
PCRE2_SIZE erroffset;
pcre2_code *re;
pcre2_match_context *mc;
pcre2_match_data *md;
int workspace[1000];
int rc;

re = pcre2_compile((PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED, 0,
  &errcode, &erroffset, NULL);
if (re == NULL)
  {
  fprintf(stderr, "compile failed %d at %lu\n", errcode, (unsigned long)erroffset);
  return 2;
  }

mc = pcre2_match_context_create(NULL);
pcre2_set_heap_limit(mc, 8);
md = pcre2_match_data_create(16, NULL);

rc = pcre2_dfa_match(re, (PCRE2_SPTR)subject, strlen(subject), 0, 0, md, mc,
  workspace, sizeof(workspace)/sizeof(workspace[0]));

printf("rc=%d\n", rc);
pcre2_match_data_free(md);
pcre2_match_context_free(mc);
pcre2_code_free(re);
return 0;
}
