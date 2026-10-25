/*
 * Probe for the curl mprintf parameter table handling.
 *
 * The format string below carries 65 sequential "%*d" conversions. Each
 * one consumes two input parameters (the width argument and the value
 * argument), so the 65th width is read from parameter index 128. curl's
 * printf parser keeps the parameter table on the stack with room for
 * MAX_PARAMETERS (128) entries, indexed 0..127.
 *
 * On an affected libcurl this aborts under AddressSanitizer with a
 * stack-buffer-overflow inside the format parser. On a correct libcurl
 * the call is rejected and returns an empty allocated string.
 *
 * Build against an ASan-instrumented static libcurl, e.g.:
 *   cc -fsanitize=address -g -I include pocs/newbug.c \
 *      <build>/lib/libcurl-d.a -o newbug -lpthread
 * then run: ./newbug
 */
#include <stdio.h>
#include <string.h>

#include <curl/curl.h>

/* report sanitizer errors with SIGABRT (rc 134) */
const char *__asan_default_options(void)
{
  return "abort_on_error=1";
}

#define Z10 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
#define Z50 Z10, Z10, Z10, Z10, Z10
#define Z130 Z50, Z50, Z10, Z10, Z10

int main(void)
{
  char fmt[65 * 3 + 1];
  char *out;
  size_t i;

  for(i = 0; i < 65; i++)
    memcpy(&fmt[i * 3], "%*d", 3);
  fmt[sizeof(fmt) - 1] = '\0';

  out = curl_maprintf(fmt, Z130);
  if(out) {
    printf("rejected cleanly, got an empty string (%zu bytes)\n",
           strlen(out));
    curl_free(out);
    return 0;
  }
  printf("curl_maprintf returned NULL\n");
  return 1;
}
