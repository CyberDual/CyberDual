/* Drives the XML declaration version check through the public API.
   The declared version below matches the XML 1.0 Fifth Edition
   VersionNum production ("1." followed by one or more digits), so the
   document is accepted by a conforming parser. */
#include <stdio.h>
#include <string.h>

#include "expat.h"

int
main(void) {
  const char doc[] = "<?xml version='1.00000000000000'?>\n<doc/>";
  XML_Parser parser = XML_ParserCreate(NULL);
  enum XML_Status status;

  if (parser == NULL) {
    fprintf(stderr, "XML_ParserCreate failed\n");
    return 2;
  }
  status = XML_Parse(parser, doc, (int)strlen(doc), XML_TRUE);
  if (status == XML_STATUS_ERROR) {
    fprintf(stderr, "parse error: %s\n",
            XML_ErrorString(XML_GetErrorCode(parser)));
    XML_ParserFree(parser);
    return 1;
  }
  XML_ParserFree(parser);
  printf("parsed OK\n");
  return 0;
}
