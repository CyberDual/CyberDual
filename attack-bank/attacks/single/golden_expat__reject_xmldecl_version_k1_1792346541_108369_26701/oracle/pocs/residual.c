#include <expat.h>

#include <string.h>

int main(void) {
  const char *const document
      = "<?xml version='1.123456789012345678'?><doc/>";
  XML_Parser parser = XML_ParserCreate(NULL);
  if (parser == NULL)
    return 2;
  XML_Parse(parser, document, (int)strlen(document), XML_TRUE);
  XML_ParserFree(parser);
  return 0;
}
