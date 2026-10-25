#include "expat.h"
#include <stdio.h>

int main(void) {
  char document[4096];
  size_t used = 0;
  XML_Parser parser = XML_ParserCreate(NULL);
  if (parser == NULL)
    return 2;
  used += (size_t)snprintf(document + used, sizeof(document) - used, "<root");
  for (int i = 0; i < 80; ++i)
    used += (size_t)snprintf(document + used, sizeof(document) - used,
                             " a%d=\"v\"", i);
  used += (size_t)snprintf(document + used, sizeof(document) - used, "/>");
  if (used >= sizeof(document))
    return 3;
  enum XML_Status status = XML_Parse(parser, document, (int)used, XML_TRUE);
  XML_ParserFree(parser);
  return status == XML_STATUS_OK ? 0 : 1;
}
