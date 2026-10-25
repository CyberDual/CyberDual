#include <stdlib.h>
#include <string.h>
#include <pcre2posix.h>

int
main(void)
{
  regex_t re;
  regmatch_t m;
  char *buf = malloc(4);

  if (buf == NULL)
    return 1;
  memcpy(buf, "abcd", 4);
  if (regcomp(&re, ".*", 0) != 0)
    return 2;
  m.rm_so = 0;
  m.rm_eo = 4;
  regexec(&re, buf, 1, &m, REG_STARTEND);
  regfree(&re);
  free(buf);
  return 0;
}
