#include <string.h>

#include "expat.h"

int
main(void) {
  XML_Parser parser = XML_ParserCreate(NULL);
  if (parser == NULL)
    return 1;

  char *buffer = XML_GetBuffer(parser, 1);
  if (buffer == NULL)
    return 1;
  buffer[0] = '<';
  if (XML_ParseBuffer(parser, 1, XML_FALSE) != XML_STATUS_OK) {
    XML_ParserFree(parser);
    return 1;
  }

  buffer = XML_GetBuffer(parser, 4096);
  if (buffer == NULL) {
    XML_ParserFree(parser);
    return 1;
  }
  memset(buffer, 'A', 4096);

  XML_ParserFree(parser);
  return 0;
}
