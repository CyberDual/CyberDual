#define PCRE2_CODE_UNIT_WIDTH 32
#include "pcre2.h"

int main(void)
{
PCRE2_UCHAR pattern[] = { '[', '!', 'a', ']', 0 };
PCRE2_UCHAR *converted = NULL;
PCRE2_SIZE length = 0;
pcre2_convert_context *context = pcre2_convert_context_create(NULL);
int error;

if (context == NULL) return 1;
if (pcre2_set_glob_separator(context, (PCRE2_UCHAR)'\\') != 0)
  {
  pcre2_convert_context_free(context);
  return 2;
  }

error = pcre2_pattern_convert(pattern, PCRE2_ZERO_TERMINATED,
  PCRE2_CONVERT_GLOB_NO_WILD_SEPARATOR, &converted, &length, context);

if (error == 0) pcre2_converted_pattern_free(converted);
pcre2_convert_context_free(context);
return error;
}
