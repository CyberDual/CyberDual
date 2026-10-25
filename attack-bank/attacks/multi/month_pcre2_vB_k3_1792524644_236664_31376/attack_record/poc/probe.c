/* Drives the PCRE2 POSIX wrapper (public API) with a REG_STARTEND match whose
range lies entirely inside an allocated buffer. On a correct library this
matches and exits 0; on the defective tree the wrapper over-reports the
subject length to pcre2_match(), which then reads past the end of the buffer
and AddressSanitizer aborts. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <pcre2posix.h>

int main(void)
{
  regex_t preg;
  regmatch_t pmatch[1];
  const size_t bufsize = 64;
  char *subject;
  int rc;

  subject = (char *)malloc(bufsize);
  if (subject == NULL)
    return 2;
  memset(subject, 'x', bufsize);

  rc = regcomp(&preg, "x*", 0);
  if (rc != 0)
  {
    fprintf(stderr, "regcomp failed: %d\n", rc);
    return 2;
  }

  /* REG_STARTEND: match only the range [rm_so, rm_eo). The range satisfies
  0 <= rm_so <= rm_eo <= bufsize, so it is entirely within the buffer. */
  pmatch[0].rm_so = 32;
  pmatch[0].rm_eo = (regoff_t)bufsize;

  rc = regexec(&preg, subject, 1, pmatch, REG_STARTEND);
  printf("regexec rc=%d rm_so=%d rm_eo=%d\n", rc, (int)pmatch[0].rm_so, (int)pmatch[0].rm_eo);

  regfree(&preg);
  free(subject);
  return (rc == 0) ? 0 : 1;
}
