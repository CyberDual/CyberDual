#include "expat.h"

#include <stddef.h>

int
main(void) {
  XML_Parser parser = XML_ParserCreate(NULL);
  if (parser == NULL)
    return 2;

  if (XML_GetBuffer(parser, 1) == NULL) {
    XML_ParserFree(parser);
    return 2;
  }

  unsigned char *buffer = XML_GetBuffer(parser, 2048);
  if (buffer == NULL) {
    XML_ParserFree(parser);
    return 2;
  }

  for (size_t i = 0; i < 2048; i += 2) {
    buffer[i] = ' ';
    buffer[i + 1] = 0;
  }
  buffer[0] = 0xff;
  buffer[1] = 0xfe;
  buffer[2] = '<';
  buffer[4] = 'a';
  buffer[6] = '>';
  buffer[2046] = 0x00;
  buffer[2047] = 0xd8;

  enum XML_Status status = XML_ParseBuffer(parser, 2048, XML_TRUE);
  XML_ParserFree(parser);
  return status == XML_STATUS_ERROR ? 0 : 1;
}
