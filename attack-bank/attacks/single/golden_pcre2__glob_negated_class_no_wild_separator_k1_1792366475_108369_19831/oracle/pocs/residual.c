#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

int main(void)
{
pcre2_convert_context *context = pcre2_convert_context_create(NULL);
PCRE2_UCHAR *converted = NULL;
PCRE2_SIZE length = 0;
int rc = 1;

if (context != NULL)
  rc = pcre2_set_glob_separator(context, '.');

if (rc == 0)
  rc = pcre2_pattern_convert((PCRE2_SPTR)"[!a]", PCRE2_ZERO_TERMINATED,
    PCRE2_CONVERT_GLOB, &converted, &length, context);

if (rc == 0)
  pcre2_converted_pattern_free(converted);
if (context != NULL)
  pcre2_convert_context_free(context);

return rc == 0 ? 0 : 2;
}
