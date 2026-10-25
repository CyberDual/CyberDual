#define PCRE2_CODE_UNIT_WIDTH 8
#include "pcre2.h"
#include <string.h>

int main(void)
{
PCRE2_UCHAR pattern[97];
PCRE2_UCHAR *buffer = NULL;
PCRE2_SIZE length = 0;
int rc;

memset(pattern, 'a', 93);
pattern[93] = '[';
pattern[94] = '!';
pattern[95] = 'b';
pattern[96] = ']';

rc = pcre2_pattern_convert(pattern, sizeof(pattern),
  PCRE2_CONVERT_GLOB_NO_WILD_SEPARATOR, &buffer, &length, NULL);

pcre2_converted_pattern_free(buffer);
return rc == 0 ? 0 : 1;
}
