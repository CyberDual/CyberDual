#include "pcre2posix.h"

int main(void)
{
  regex_t re;
  regmatch_t match[1];
  char subject[] = "xxxa";
  if (pcre2_regcomp(&re, "b", 0) != 0)
    return 1;
  match[0].rm_so = 3;
  match[0].rm_eo = 4;
  (void)pcre2_regexec(&re, subject, 1, match, REG_STARTEND);
  pcre2_regfree(&re);
  return 0;
}
