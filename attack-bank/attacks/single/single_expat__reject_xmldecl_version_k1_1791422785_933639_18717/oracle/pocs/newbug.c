#include <string.h>

#include "expat.h"

int
main(void) {
  static const char document[]
      = "<?xml version='1.0000000000000000000000000000000000000000000000000000000000000000'?><root/>";
  XML_Parser parser = XML_ParserCreate(NULL);
  enum XML_Status status = XML_Parse(parser, document, (int)strlen(document), 1);
  XML_ParserFree(parser);
  return status == XML_STATUS_OK ? 0 : 1;
}
