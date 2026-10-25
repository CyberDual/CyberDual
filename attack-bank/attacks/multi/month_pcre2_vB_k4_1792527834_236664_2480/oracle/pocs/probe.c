/* Drives the POSIX wrapper's REG_STARTEND path with an in-contract,
   deliberately non-NUL-terminated subject buffer. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pcre2posix.h>

int main(void)
{
  regex_t re;
  regmatch_t m[1];
  const size_t total = 256;
  char *subject;
  int rc;

  rc = regcomp(&re, "a+Q", 0);
  if (rc != 0)
  {
    fprintf(stderr, "regcomp failed: %d\n", rc);
    return 1;
  }

  subject = (char *)malloc(total);
  if (subject == NULL)
    return 1;
  memset(subject, 'a', total); /* no NUL: REG_STARTEND supplies the bounds */

  /* In-contract: 0 <= rm_so <= rm_eo <= buffer size. */
  m[0].rm_so = 128;
  m[0].rm_eo = (regoff_t)total;

  rc = regexec(&re, subject, 1, m, REG_STARTEND);

  regfree(&re);
  free(subject);

  if (rc != REG_NOMATCH)
  {
    fprintf(stderr, "unexpected regexec result: %d\n", rc);
    return 1;
  }
  return 0;
}
