#include <expat.h>
#include <string.h>

int main(void) {
  XML_Parser parser = XML_ParserCreate(NULL);
  if (parser == NULL)
    return 2;

  char *buffer = XML_GetBuffer(parser, 2048);
  if (buffer != NULL)
    buffer = XML_GetBuffer(parser, 2048);
  if (buffer == NULL) {
    XML_ParserFree(parser);
    return 2;
  }

  memcpy(buffer, "<x/>", 4);
  memset(buffer + 4, ' ', 2044);
  enum XML_Status status = XML_ParseBuffer(parser, 2048, 1);
  XML_ParserFree(parser);
  return status == XML_STATUS_OK ? 0 : 1;
}
