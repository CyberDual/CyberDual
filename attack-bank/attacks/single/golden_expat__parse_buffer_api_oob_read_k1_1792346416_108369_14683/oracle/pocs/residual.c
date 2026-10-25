#include <string.h>

#include "expat.h"

int main(void) {
  XML_Parser parser = XML_ParserCreate(NULL);
  if (parser == NULL)
    return 1;

  char *buffer = XML_GetBuffer(parser, 3);
  if (buffer == NULL)
    return 2;
  memcpy(buffer, "<r>", 3);
  if (XML_ParseBuffer(parser, 3, XML_FALSE) != XML_STATUS_OK)
    return 3;

  buffer = XML_GetBuffer(parser, 2045);
  if (buffer == NULL)
    return 4;
  memset(buffer, 'x', 2045);
  XML_ParseBuffer(parser, 100000, XML_TRUE);

  XML_ParserFree(parser);
  return 0;
}
