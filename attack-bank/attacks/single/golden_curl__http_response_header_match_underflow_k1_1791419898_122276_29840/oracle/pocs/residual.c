/* Residual-defect PoC for Curl_compareheader().
 *
 * Curl_compareheader() is libcurl's checker for whether a server response
 * header (e.g. "Transfer-Encoding:" / "Content-Encoding:") contains a given
 * token within its comma separated value list. The untrusted input is the
 * raw response header line coming off the wire.
 *
 * This probe hands it one such header line, exactly as the response parser
 * would, where the value ends in a comma followed by trailing blanks. No
 * network is required: the crafted header bytes are the only thing that
 * differs from an ordinary valid header.
 *
 * The header line is placed in an exact-sized heap allocation so that any
 * read past its terminating NUL lands in the allocator red zone and is
 * reported by AddressSanitizer.
 */

#include <curl/curl.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

/* internal libcurl entry point exercised by the response header parser */
extern bool Curl_compareheader(const char *headerline,
                               const char *header, const size_t hlen,
                               const char *content, const size_t clen);

int main(void)
{
  /* a header whose value ends with a comma and trailing blanks */
  static const char line[] = "Encoding: gzipped,  ";
  const char *header = "Encoding:";
  const char *content = "gzip";

  size_t n = strlen(line);
  char *buf = (char *)malloc(n + 1);  /* exact size, NUL at buf[n] */
  memcpy(buf, line, n + 1);

  bool r = Curl_compareheader(buf, header, strlen(header),
                              content, strlen(content));
  free(buf);
  return r ? 1 : 0;
}
