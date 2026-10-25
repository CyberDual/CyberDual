#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pcre2posix.h>

int main(void)
{
  regex_t re;
  regmatch_t pmatch[2];
  char *subject;

  if (regcomp(&re, "a+", 0) != 0)
  {
    fprintf(stderr, "regcomp failed\n");
    return 1;
  }

  /* An 8-byte, heap-allocated, non-NUL-terminated subject "aaaaaaaa". */
  subject = (char *)malloc(8);
  if (subject == NULL)
    return 1;
  memset(subject, 'a', 8);

  /* REG_STARTEND: the subject is the half-open range [string + rm_so,
  string + rm_eo) = bytes 4..8 of the buffer. Per the documented contract
  rm_eo points to the first character beyond the string. The library must
  not read at or beyond string + rm_eo. */
  pmatch[0].rm_so = 4;
  pmatch[0].rm_eo = 8;
  if (regexec(&re, subject, 2, pmatch, REG_STARTEND) != 0)
  {
    fprintf(stderr, "regexec failed\n");
    regfree(&re);
    free(subject);
    return 1;
  }
  printf("matched so=%d eo=%d\n", (int)pmatch[0].rm_so, (int)pmatch[0].rm_eo);

  regfree(&re);
  free(subject);
  return 0;
}
