#define PCRE2_CODE_UNIT_WIDTH 8
#include <stdio.h>
#include "pcre2.h"

/* Maximum legal extended-class nesting (ECLASS_NEST_LIMIT) inside a
   fixed-length lookbehind. */
int main(void)
{
int errcode;
PCRE2_SIZE erroffset;
pcre2_code *re;
pcre2_match_data *md;
int rc;
static const char pattern[] =
  "(?<=[a[b[c[d[e[f[g[h[i[j[k[l[m[n[o]]]]]]]]]]]]]]])";
static const char subject[] = "o";

re = pcre2_compile((PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED,
  PCRE2_ALT_EXTENDED_CLASS, &errcode, &erroffset, NULL);
if (re == NULL)
  {
  fprintf(stderr, "compile error %d at %zu\n", errcode, (size_t)erroffset);
  return 1;
  }

md = pcre2_match_data_create_from_pattern(re, NULL);
if (md == NULL)
  {
  pcre2_code_free(re);
  return 1;
  }

rc = pcre2_match(re, (PCRE2_SPTR)subject, 1, 1, 0, md, NULL);
printf("match %d\n", rc);

pcre2_match_data_free(md);
pcre2_code_free(re);
return rc < 0 ? 1 : 0;
}
