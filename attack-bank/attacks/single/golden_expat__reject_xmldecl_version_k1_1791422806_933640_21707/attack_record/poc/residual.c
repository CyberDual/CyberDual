#include "expat.h"
#include <stdio.h>
#include <string.h>

int
main(void) {
  static const char document[]
      = "<?xml version='1.00000000000000000000000000000000'?>\n<doc/>";
  XML_Parser parser = XML_ParserCreate(NULL);
  if (parser == NULL)
    return 2;
  enum XML_Status status
      = XML_Parse(parser, document, (int)strlen(document), XML_TRUE);
  if (status != XML_STATUS_OK)
    fprintf(stderr, "parse error: %d\n", XML_GetErrorCode(parser));
  XML_ParserFree(parser);
  return status == XML_STATUS_OK ? 0 : 1;
}
