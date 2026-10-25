#include <expat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void) {
  enum { length = 2048 };
  char *input = malloc(length);
  XML_Parser parser = XML_ParserCreate(NULL);
  if (!input || !parser) return 2;
  memcpy(input, "<root>", 6);
  memset(input + 6, 'x', length - 13);
  memcpy(input + length - 7, "</root>", 7);
  if (XML_Parse(parser, input, length, XML_TRUE) != XML_STATUS_OK) return 3;
  printf("%llu\n", (unsigned long long)XML_GetCurrentLineNumber64(parser));
  XML_ParserFree(parser);
  free(input);
  return 0;
}
