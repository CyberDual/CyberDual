/*************************************************
* Benchmark probe: POSIX wrapper out-of-bounds   *
* write when nmatch is smaller than the number   *
* of captured pairs                              *
*************************************************/

/* Self-contained: drives only the public POSIX wrapper API
(regcomp/regexec/regfree) with in-contract input. On a tree where
pcre2_regexec writes back more pairs than the caller's pmatch vector
holds, AddressSanitizer aborts with a stack-buffer-overflow. On a
correct tree this program runs to completion and exits 0. */

#include <stdio.h>
#include <string.h>

#include <pcre2posix.h>

int main(void)
{
  regex_t re;
  regmatch_t pmatch[2];
  char errbuf[256];
  const char *pattern = "(a+)(b+)(c+)(d+)";
  const char *subject = "aabbccdd";
  int rc;

  rc = regcomp(&re, pattern, 0);
  if (rc != 0)
  {
    regerror(rc, &re, errbuf, sizeof(errbuf));
    fprintf(stderr, "regcomp failed: %s\n", errbuf);
    return 1;
  }

  /* Perfectly ordinary POSIX usage: the caller supplies a vector of two
  slots even though the pattern has four capturing groups (five reported
  pairs). The library must never write past pmatch[nmatch-1]. */

  memset(pmatch, 0, sizeof(pmatch));
  rc = regexec(&re, subject, 2, pmatch, 0);

  printf("regexec returned %d, match at %d..%d\n", rc, (int)pmatch[0].rm_so,
         (int)pmatch[0].rm_eo);

  regfree(&re);
  return 0;
}
