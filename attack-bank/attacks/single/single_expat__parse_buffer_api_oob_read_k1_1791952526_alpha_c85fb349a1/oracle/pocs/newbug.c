#include <stdlib.h>
#include <string.h>

#include "expat.h"

int
main(void) {
  enum { BUFFER_LEN = 2048 };

  XML_Parser parser = XML_ParserCreate(NULL);
  if (parser == NULL)
    return 1;

  char *const buffer = XML_GetBuffer(parser, BUFFER_LEN);
  if (buffer == NULL) {
    XML_ParserFree(parser);
    return 1;
  }

  size_t off = 0;
  memcpy(buffer + off, "<r>", 3);
  off += 3;
  for (int i = 0; i < 280; i++) {
    memcpy(buffer + off, "<a/>", 4);
    off += 4;
  }
  memcpy(buffer + off, "<!--", 4);
  off += 4;
  memset(buffer + off, 'a', BUFFER_LEN - off);

  if (XML_ParseBuffer(parser, BUFFER_LEN, XML_FALSE) != XML_STATUS_OK) {
    XML_ParserFree(parser);
    return 1;
  }

  void *const more = XML_GetBuffer(parser, 1);
  if (more == NULL) {
    XML_ParserFree(parser);
    return 1;
  }

  XML_ParserFree(parser);
  return 0;
}
