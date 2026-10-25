/* PoC: parse a document whose XML declaration carries a long version
   number, through the public XML_Parse API. */
#include <stdio.h>
#include <string.h>
#include "expat.h"

int
main(void) {
  const char *doc = "<?xml version=\"1.00000000000000000000\"?>\n<doc/>";
  XML_Parser parser = XML_ParserCreate(NULL);
  enum XML_Status status;
  enum XML_Error error;

  if (parser == NULL) {
    fprintf(stderr, "cannot create parser\n");
    return 1;
  }
  status = XML_Parse(parser, doc, (int)strlen(doc), XML_TRUE);
  error = XML_GetErrorCode(parser);
  printf("XML_Parse status=%d error=%s\n", status,
         error == XML_ERROR_NONE ? "none" : XML_ErrorString(error));
  XML_ParserFree(parser);
  return 0;
}
