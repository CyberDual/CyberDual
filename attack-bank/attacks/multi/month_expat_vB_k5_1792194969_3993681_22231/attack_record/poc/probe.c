#include <string.h>

#include <expat.h>

int
main(void) {
  XML_Parser parser = XML_ParserCreateNS(NULL, ' ');
  if (parser == NULL)
    return 1;
  XML_SetReturnNSTriplet(parser, XML_TRUE);
  const char *doc
      = "<p:abcdefghijklmnopqrstuvwxyzabc xmlns:p=\"urn:example\">"
        "z"
        "</p:abcdefghijklmnopqrstuvwxyzabc>";
  const enum XML_Status status
      = XML_Parse(parser, doc, (int)strlen(doc), XML_TRUE);
  XML_ParserFree(parser);
  return status == XML_STATUS_OK ? 0 : 2;
}
