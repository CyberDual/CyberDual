#include <stdio.h>
#include <string.h>

#include "expat.h"

int
main(void) {
  char version[81];
  char doc[160];
  XML_Parser parser;
  int i;

  version[0] = '1';
  version[1] = '.';
  for (i = 2; i < 80; i++)
    version[i] = '1';
  version[80] = '\0';

  if (snprintf(doc, sizeof(doc), "<?xml version='%s'?><doc/>", version)
      >= (int)sizeof(doc))
    return 2;

  parser = XML_ParserCreate(NULL);
  if (! parser)
    return 2;
  if (XML_Parse(parser, doc, (int)strlen(doc), XML_TRUE) != XML_STATUS_OK) {
    fprintf(stderr, "parse error %d\n", (int)XML_GetErrorCode(parser));
    XML_ParserFree(parser);
    return 1;
  }
  XML_ParserFree(parser);
  return 0;
}
