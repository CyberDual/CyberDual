/* PoC: parses a document whose XML declaration carries a syntactically
   valid but unusually long version number ("1." followed by many digits,
   which matches production VersionNum: '1.' [0-9]+).
*/
#include <stdio.h>
#include <string.h>

#include "expat.h"

int
main(void) {
  char doc[512];
  const int digits = 120;
  int n;
  XML_Parser parser;
  enum XML_Status status;

  n = sprintf(doc, "<?xml version=\"1.");
  memset(doc + n, '0', (size_t)digits);
  n += digits;
  sprintf(doc + n, "\"?>\n<doc/>\n");
  n += (int)strlen(doc + n);

  parser = XML_ParserCreate(NULL);
  if (parser == NULL) {
    fprintf(stderr, "XML_ParserCreate failed\n");
    return 2;
  }
  status = XML_Parse(parser, doc, n, XML_TRUE);
  if (status == XML_STATUS_ERROR) {
    fprintf(stderr, "parse error at line %lu: %s\n",
            XML_GetCurrentLineNumber(parser),
            XML_ErrorString(XML_GetErrorCode(parser)));
    XML_ParserFree(parser);
    return 1;
  }
  XML_ParserFree(parser);
  printf("parsed %d bytes ok\n", n);
  return 0;
}
