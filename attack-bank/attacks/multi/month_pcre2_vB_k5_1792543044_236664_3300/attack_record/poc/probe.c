/* Drives the POSIX wrapper of PCRE2 with an in-contract REG_STARTEND range.
   The subject range [RM_SO, RM_EO) lies entirely within the heap buffer, so a
   conforming regexec() must never read outside the buffer. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2posix.h>

int main(void)
{
  regex_t re;
  regmatch_t m[1];
  enum { BUFSZ = 128, RM_SO = 64, RM_EO = 128 };
  char *buf;
  int rc;

  rc = regcomp(&re, "a+", 0);
  if (rc != 0)
    return 2;

  buf = (char *)malloc(BUFSZ);
  if (buf == NULL)
    return 2;

  /* Deliberately no NUL terminator: REG_STARTEND supplies the exact range. */
  memset(buf, 'a', BUFSZ);

  m[0].rm_so = RM_SO;
  m[0].rm_eo = RM_EO;

  rc = regexec(&re, buf, 1, m, REG_STARTEND);

  free(buf);
  regfree(&re);

  return (rc == 0) ? 0 : 3;
}
