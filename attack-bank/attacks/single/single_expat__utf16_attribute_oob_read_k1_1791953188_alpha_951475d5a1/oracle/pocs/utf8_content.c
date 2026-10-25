#define XML_UNICODE
#include <expat.h>

static void
handle_data(void *userData, const XML_Char *text, int length) {
  (void)userData;
  (void)text;
  (void)length;
}

int
main(void) {
  unsigned char text[3 + 1023 + 4 + 4];
  size_t at = 0;
  XML_Parser parser;

  text[at++] = '<';
  text[at++] = 'r';
  text[at++] = '>';
  for (int i = 0; i < 1023; ++i)
    text[at++] = 'A';
  text[at++] = 0xf0;
  text[at++] = 0x9d;
  text[at++] = 0x84;
  text[at++] = 0x9e;
  text[at++] = '<';
  text[at++] = '/';
  text[at++] = 'r';
  text[at++] = '>';

  parser = XML_ParserCreate(NULL);
  if (parser == NULL)
    return 1;

  XML_SetCharacterDataHandler(parser, handle_data);
  if (XML_Parse(parser, (const char *)text, (int)at, XML_TRUE)
      == XML_STATUS_ERROR) {
    XML_ParserFree(parser);
    return 1;
  }

  XML_ParserFree(parser);
  return 0;
}
