/* Probe: drives the POSIX wrapper (pcre2posix.h public API) with in-contract
   input. REG_STARTEND is the documented BSD extension for passing a subject
   that is not NUL-terminated: the subject is string + rm_so .. string + rm_eo.
   Here rm_so = 12, rm_eo = 16 over a 16-byte heap buffer with no NUL byte,
   i.e. the 4-byte subject "aaaa" at the end of the allocation. The pattern
   never matches, so a correct library scans exactly those 4 bytes and returns
   REG_NOMATCH. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <pcre2posix.h>

int main(void)
{
  regex_t re;
  regmatch_t m[1];
  char *subject;
  int rc;

  subject = (char *)malloc(16);
  if (subject == NULL)
    return 2;
  memset(subject, 'a', 16); /* deliberately no NUL terminator */

  rc = regcomp(&re, "aaaaq", 0);
  if (rc != 0)
  {
    free(subject);
    return 2;
  }

  m[0].rm_so = 12;
  m[0].rm_eo = 16;

  rc = regexec(&re, subject, 1, m, REG_STARTEND);

  regfree(&re);
  free(subject);

  if (rc != REG_NOMATCH)
  {
    fprintf(stderr, "unexpected regexec result: %d\n", rc);
    return 1;
  }

  /* Skip atexit processing; all checks are complete. */
  _exit(0);
}
