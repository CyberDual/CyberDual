/* Proof-of-concept probe for the PCRE2 POSIX wrapper.
 *
 * It drives the public POSIX API (regcomp/regexec, declared in pcre2posix.h)
 * with fully in-contract input and, on a correct library, simply reports
 * REG_NOMATCH.
 *
 * REG_STARTEND is a documented BSD extension: pmatch[0].rm_so and
 * pmatch[0].rm_eo give the start and end byte offsets of the region of
 * "string" to be searched. The caller must provide a buffer that is valid for
 * offsets [0, rm_eo); rm_so may be any value in [0, rm_eo]. Here we hand
 * regexec a heap buffer of exactly rm_eo bytes and ask it to search the
 * sub-region [rm_so, rm_eo) with rm_so > 0 - a textbook in-contract use.
 *
 * A correct implementation searches exactly rm_eo - rm_so bytes starting at
 * string + rm_so and never reads at or beyond string + rm_eo.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pcre2posix.h"

int
main(void)
{
  regex_t re;
  regmatch_t pmatch[1];
  int rc;
  size_t n = 64;
  char *subject;

  /* A one-character pattern that will not be present in the subject, so the
     match engine scans the whole search region before reporting no match. */
  rc = regcomp(&re, "z", 0);
  if (rc != 0)
  {
    fprintf(stderr, "regcomp failed: %d\n", rc);
    return 2;
  }

  /* Exactly n bytes; no trailing NUL (REG_STARTEND does not require one). */
  subject = (char *)malloc(n);
  if (subject == NULL)
    return 2;
  memset(subject, 'a', n);

  /* In-contract: search subject[8 .. 64). rm_eo == n is the end of the buffer,
     rm_so == 8 skips a prefix. */
  pmatch[0].rm_so = 8;
  pmatch[0].rm_eo = (int)n;

  rc = regexec(&re, subject, 1, pmatch, REG_STARTEND);

  free(subject);
  regfree(&re);

  printf("regexec rc=%d\n", rc);
  return 0;
}
