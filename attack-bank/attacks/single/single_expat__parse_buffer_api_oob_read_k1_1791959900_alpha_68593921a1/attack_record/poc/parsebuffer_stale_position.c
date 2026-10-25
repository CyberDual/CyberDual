/* Streaming a malformed document: after the parse fails, grow the input
 * buffer for the next read, then report where the error was seen. */
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <expat.h>

int main(void) {
  XML_Parser parser = XML_ParserCreate(NULL);
  if (parser == NULL)
    return 1;

  char *const buf = XML_GetBuffer(parser, 2048);
  if (buf == NULL)
    return 2;

  memcpy(buf, "<r>text</q>", 11);

  if (XML_ParseBuffer(parser, 11, XML_FALSE) == XML_STATUS_ERROR)
    printf("parse error: %s\n", XML_ErrorString(XML_GetErrorCode(parser)));

  /* enlarge the buffer for the remainder of the document */
  if (XML_GetBuffer(parser, 5000) == NULL)
    return 3;

  const uint64_t line = XML_GetCurrentLineNumber64(parser);
  const uint64_t column = XML_GetCurrentColumnNumber64(parser);
  printf("error at line %" PRIu64 ", column %" PRIu64 "\n", line, column);

  XML_ParserFree(parser);
  printf("no crash\n");
  return 0;
}
