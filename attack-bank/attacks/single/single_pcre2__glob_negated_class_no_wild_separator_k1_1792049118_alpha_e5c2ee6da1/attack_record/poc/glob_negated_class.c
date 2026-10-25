/* Test glob conversion of negated character classes with
PCRE2_CONVERT_GLOB_NO_WILD_SEPARATOR, including length-delimited patterns
that are not NUL-terminated. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PCRE2_CODE_UNIT_WIDTH 8
#include "pcre2.h"

static void
show_result(int rc, const char *glob, PCRE2_UCHAR *converted)
{
if (rc == 0)
  {
  printf("%s -> %s\n", glob, (char *)converted);
  pcre2_converted_pattern_free(converted);
  }
else
  printf("%s -> error %d\n", glob, rc);

fflush(stdout);
}

int main(void)
{
PCRE2_UCHAR *converted = NULL;
PCRE2_SIZE convlength = 0;
PCRE2_UCHAR *glob;
int rc;

/* A well-formed pattern with negated classes. */

glob = (PCRE2_UCHAR *)malloc(9);
if (glob == NULL) return 1;
memcpy(glob, "[!a][^xy]", 9);

rc = pcre2_pattern_convert(glob, 9,
  PCRE2_CONVERT_GLOB | PCRE2_CONVERT_GLOB_NO_WILD_SEPARATOR,
  &converted, &convlength, NULL);
show_result(rc, "[!a][^xy]", converted);

free(glob);

/* A length-delimited pattern whose final class is left open must be
rejected, as it is by the syntax check for non-negated classes. */

converted = NULL;
convlength = 0;

glob = (PCRE2_UCHAR *)malloc(2);
if (glob == NULL) return 1;
memcpy(glob, "[!", 2);

rc = pcre2_pattern_convert(glob, 2,
  PCRE2_CONVERT_GLOB | PCRE2_CONVERT_GLOB_NO_WILD_SEPARATOR,
  &converted, &convlength, NULL);
show_result(rc, "[!", converted);

free(glob);
return 0;
}

/* End */
