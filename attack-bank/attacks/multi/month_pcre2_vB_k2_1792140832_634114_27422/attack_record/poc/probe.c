#include <stdlib.h>
#include <string.h>

#include <pcre2posix.h>

/* REG_STARTEND subjects are bounded by rm_so/rm_eo and need not be
NUL-terminated. A one-byte pattern matches inside that span. */
int
main(void)
{
  regex_t re;
  regmatch_t m;
  char *buf;

  if (regcomp(&re, ".", 0) != 0)
    return 0;

  buf = malloc(8);
  if (buf == NULL)
    return 0;
  memset(buf, 'a', 8);

  m.rm_so = 1;
  m.rm_eo = 8;
  (void)regexec(&re, buf, 1, &m, REG_STARTEND);

  regfree(&re);
  free(buf);
  return 0;
}
