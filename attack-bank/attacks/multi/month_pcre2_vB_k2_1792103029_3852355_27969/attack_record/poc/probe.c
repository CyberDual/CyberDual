#include "pcre2posix.h"
#include <stdlib.h>
#include <string.h>

int main(void)
{
  regex_t re;
  regmatch_t range[1];
  char *subject = malloc(64);
  if (subject == NULL)
    return 1;
  memset(subject, 'a', 64);
  if (pcre2_regcomp(&re, "z", 0) != 0)
  {
    free(subject);
    return 1;
  }
  range[0].rm_so = 40;
  range[0].rm_eo = 50;
  (void)pcre2_regexec(&re, subject, 1, range, REG_STARTEND);
  pcre2_regfree(&re);
  free(subject);
  return 0;
}
