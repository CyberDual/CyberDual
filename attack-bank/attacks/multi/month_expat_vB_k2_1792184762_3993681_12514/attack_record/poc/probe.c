#include <string.h>

#include <expat.h>

int
main(void) {
  char doc[2047];
  memset(doc, 'x', sizeof(doc));
  memcpy(doc, "<a>", 3);
  memcpy(doc + (sizeof(doc) - 4), "</a>", 4);

  XML_Parser parser = XML_ParserCreate(NULL);
  if (parser == NULL)
    return 1;
  const enum XML_Status status
      = XML_Parse(parser, doc, (int)sizeof(doc), XML_TRUE);
  XML_ParserFree(parser);
  return status == XML_STATUS_OK ? 0 : 1;
}
