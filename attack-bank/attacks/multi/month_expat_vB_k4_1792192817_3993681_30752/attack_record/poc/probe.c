#include <string.h>

#include "expat.h"

int
main(void) {
  static const char prefix[] = "<?xml version=\"1.0\" encoding=\"";
  static const char suffix[] = "\"?><a/>";
  char doc[256];
  char *p = doc;
  XML_Parser parser;
  enum XML_Status status;

  memcpy(p, prefix, sizeof(prefix) - 1);
  p += sizeof(prefix) - 1;
  memset(p, 'A', 128);
  p += 128;
  memcpy(p, suffix, sizeof(suffix) - 1);
  p += sizeof(suffix) - 1;

  parser = XML_ParserCreate(NULL);
  if (parser == NULL)
    return 1;
  status = XML_Parse(parser, doc, (int)(p - doc), 1);
  XML_ParserFree(parser);
  return status == XML_STATUS_OK ? 0 : 2;
}
