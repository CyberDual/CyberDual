#include "expat.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

static size_t appendAttribute(char *destination, size_t index, size_t number) {
  const int written
      = sprintf(destination + index, " a%02zu='%zu'", number, number);
  return index + (size_t)written;
}

int main(void) {
  char document[1024] = "<root><first";
  size_t length = strlen(document);

  for (size_t i = 0; i < 17; ++i) {
    length = appendAttribute(document, length, i);
  }
  memcpy(document + length, "/><second", 9);
  length += 9;

  for (size_t i = 0; i < 33; ++i) {
    length = appendAttribute(document, length, i);
  }
  memcpy(document + length, "/></root>", 9);
  length += 9;

  XML_Parser parser = XML_ParserCreate(NULL);
  if (parser == NULL) {
    return 2;
  }

  const enum XML_Status status
      = XML_Parse(parser, document, (int)length, XML_TRUE);
  XML_ParserFree(parser);

  return status == XML_STATUS_OK ? 0 : 3;
}
