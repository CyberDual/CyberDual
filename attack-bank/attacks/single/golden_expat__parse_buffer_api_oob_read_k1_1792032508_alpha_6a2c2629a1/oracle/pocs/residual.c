#include <stdio.h>
#include <string.h>

#include "expat.h"

/* XML_GetBuffer grows from INIT_BUFFER_SIZE (1024) by doubling at least
 * once, so the first allocation large enough for 2048 bytes is 2048.
 */
enum { BUF_SIZE = 2048 };

int
main(void) {
  XML_Parser parser = XML_ParserCreate(NULL);
  if (parser == NULL) {
    fprintf(stderr, "XML_ParserCreate failed\n");
    return 2;
  }

  void *const buffer = XML_GetBuffer(parser, BUF_SIZE);
  if (buffer == NULL) {
    fprintf(stderr, "XML_GetBuffer failed: %s\n",
            XML_ErrorString(XML_GetErrorCode(parser)));
    XML_ParserFree(parser);
    return 2;
  }

  /* Partial comment: every byte is defined, and the token cannot end before
   * the claimed limit, so the scanner walks the whole range. */
  memset(buffer, 'a', BUF_SIZE);
  memcpy(buffer, "<!--", 4);

  /* Consume a prefix. The allocation still holds BUF_SIZE bytes, but the
   * unoccupied tail is now shorter than that. */
  const int prefix = 16;
  if (XML_ParseBuffer(parser, prefix, XML_FALSE) != XML_STATUS_OK) {
    fprintf(stderr, "prefix parse failed: %s\n",
            XML_ErrorString(XML_GetErrorCode(parser)));
    XML_ParserFree(parser);
    return 2;
  }

  /* Fits the allocation (len == BUF_SIZE) but overruns the unoccupied tail
   * by `prefix` bytes. */
  const enum XML_Status status
      = XML_ParseBuffer(parser, BUF_SIZE, XML_FALSE);
  fprintf(stderr, "status=%d err=%s\n", (int)status,
          XML_ErrorString(XML_GetErrorCode(parser)));
  XML_ParserFree(parser);
  return status == XML_STATUS_ERROR ? 0 : 1;
}
