#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
  PCRE2_UCHAR *buffer = malloc(7);
  PCRE2_SIZE length = 7;
  int rc;

  if (buffer == NULL) return 2;

  rc = pcre2_pattern_convert((PCRE2_SPTR)"[!a]", PCRE2_ZERO_TERMINATED,
    PCRE2_CONVERT_GLOB_NO_WILD_SEPARATOR, &buffer, &length, NULL);

  printf("rc=%d length=%zu\n", rc, (size_t)length);
  free(buffer);
  return rc == PCRE2_ERROR_NOMEMORY ? 0 : 1;
}
