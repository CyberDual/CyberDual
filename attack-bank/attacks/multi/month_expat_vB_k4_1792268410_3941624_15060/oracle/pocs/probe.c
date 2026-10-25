#include "expat.h"

#include <string.h>

int
main(void) {
  XML_Parser parser = XML_ParserCreate(NULL);
  if (parser == NULL)
    return 1;

  char input[2048];
  memset(input, ' ', sizeof(input));
  memcpy(input, "<r>", 3);
  memcpy(input + sizeof(input) - 4, "</r>", 4);

  void *const buffer = XML_GetBuffer(parser, (int)sizeof(input));
  if (buffer == NULL)
    return 2;

  memcpy(buffer, input, sizeof(input));
  XML_ParseBuffer(parser, (int)sizeof(input), XML_TRUE);
  XML_ParserFree(parser);
  return 0;
}
