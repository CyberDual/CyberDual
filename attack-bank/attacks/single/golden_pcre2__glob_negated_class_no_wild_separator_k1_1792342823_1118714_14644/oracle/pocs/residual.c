/* PoC: convert a negated glob class with a separator that is a regular
   expression metacharacter, through the documented public convert API. */

#include <stdio.h>
#include <string.h>

#define PCRE2_STATIC
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

int main(void)
{
pcre2_convert_context *ccontext;
PCRE2_UCHAR *buffer = NULL;
PCRE2_SIZE bufflen = 0;
PCRE2_SPTR pattern = (PCRE2_SPTR)"[!a]";
int rc;

ccontext = pcre2_convert_context_create(NULL);
if (ccontext == NULL)
  {
  fprintf(stderr, "convert context create failed\n");
  return 2;
  }

/* A glob separator that needs escaping in regular expression output. */
rc = pcre2_set_glob_separator(ccontext, '.');
if (rc != 0)
  {
  fprintf(stderr, "set glob separator failed: %d\n", rc);
  return 2;
  }

rc = pcre2_pattern_convert(pattern, PCRE2_ZERO_TERMINATED,
  PCRE2_CONVERT_GLOB, &buffer, &bufflen, ccontext);

if (rc != 0)
  {
  fprintf(stderr, "convert error %d at offset %lu\n", rc,
    (unsigned long)bufflen);
  pcre2_convert_context_free(ccontext);
  return 1;
  }

printf("converted: %.*s\n", (int)bufflen, (const char *)buffer);

pcre2_converted_pattern_free(buffer);
pcre2_convert_context_free(ccontext);
return 0;
}
