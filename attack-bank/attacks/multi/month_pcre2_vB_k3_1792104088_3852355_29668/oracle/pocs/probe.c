#include <stdlib.h>
#include "pcre2posix.h"

int main(void)
{
  regex_t regex;
  regmatch_t match[1];
  char *subject = malloc(4);
  int result;

  if (subject == NULL)
    return 1;
  subject[0] = 'a';
  subject[1] = 'a';
  subject[2] = 'a';
  subject[3] = 'a';

  result = regcomp(&regex, "z", 0);
  if (result != 0)
  {
    free(subject);
    return 2;
  }

  match[0].rm_so = 3;
  match[0].rm_eo = 4;
  result = regexec(&regex, subject, 1, match, REG_STARTEND);
  regfree(&regex);
  free(subject);
  return result == REG_NOMATCH ? 0 : 3;
}
