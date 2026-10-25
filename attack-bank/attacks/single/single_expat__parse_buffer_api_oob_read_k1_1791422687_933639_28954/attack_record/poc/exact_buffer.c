#include "expat.h"

#include <string.h>

int
main(void) {
  XML_Parser parser = XML_ParserCreate(NULL);
  if (parser == NULL)
    return 2;

  char *buffer = XML_GetBuffer(parser, 2048);
  if (buffer == NULL)
    return 2;

  memcpy(buffer, "<r>", 3);
  memset(buffer + 3, 'a', 2041);
  memcpy(buffer + 2044, "</r>", 4);

  enum XML_Status status = XML_ParseBuffer(parser, 2048, XML_TRUE);
  XML_ParserFree(parser);
  return status == XML_STATUS_OK ? 0 : 2;
}
