/* Probe: drives the XML_GetBuffer/XML_ParseBuffer streaming API.
 *
 * A first chunk leaves a partial comment buffered (some bytes consumed,
 * some still waiting in the buffer).  A second XML_ParseBuffer then asks
 * the parser to accept slightly more bytes than the free tail of the
 * internal buffer can hold.  A robust library must reject that second
 * call with XML_ERROR_INVALID_ARGUMENT instead of letting the tokenizer
 * run past the end of the allocated buffer.
 */
#include <stdio.h>
#include <string.h>

#include "expat.h"

int
main(void) {
  XML_Parser parser = XML_ParserCreate(NULL);
  if (parser == NULL)
    return 2;

  enum { kCapacity = 2048 };
  char *const buf = (char *)XML_GetBuffer(parser, kCapacity);
  if (buf == NULL)
    return 2;

  /* Fill the whole granted region: closed root element, then an
     unterminated comment opener, then inert character filler. */
  memcpy(buf, "<a/>", 4);
  memcpy(buf + 4, "<!--", 4);
  memset(buf + 8, 'a', kCapacity - 8);

  /* First chunk: consumes "<a/>", buffers the partial comment opener. */
  if (XML_ParseBuffer(parser, 8, XML_FALSE) != XML_STATUS_OK) {
    fprintf(stderr, "first XML_ParseBuffer failed: %s\n",
            XML_ErrorString(XML_GetErrorCode(parser)));
    XML_ParserFree(parser);
    return 2;
  }

  /* Second chunk: len exceeds the free tail of the buffer (2040 bytes
     free, 2042 claimed).  Must be rejected, not crash. */
  if (XML_ParseBuffer(parser, 2042, XML_FALSE) == XML_STATUS_OK) {
    fprintf(stderr, "second XML_ParseBuffer unexpectedly accepted\n");
    XML_ParserFree(parser);
    return 1;
  }
  if (XML_GetErrorCode(parser) != XML_ERROR_INVALID_ARGUMENT) {
    fprintf(stderr, "unexpected error code: %s\n",
            XML_ErrorString(XML_GetErrorCode(parser)));
    XML_ParserFree(parser);
    return 1;
  }

  XML_ParserFree(parser);
  printf("OK: oversized XML_ParseBuffer rejected cleanly\n");
  return 0;
}
