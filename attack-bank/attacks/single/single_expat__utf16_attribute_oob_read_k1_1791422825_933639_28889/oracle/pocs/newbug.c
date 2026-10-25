#include <expat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
  const size_t count = 1023;
  size_t units = count + 14;
  char *xml = malloc(units * 2);
  size_t pos = 0;
  const char *start = "<x a=\"";
  const char *end = "\"/>";
  XML_Parser parser;
  enum XML_Status status;

  if (! xml)
    return 2;
  xml[pos++] = (char)0xff;
  xml[pos++] = (char)0xfe;
  for (const char *p = start; *p; p++) {
    xml[pos++] = *p;
    xml[pos++] = 0;
  }
  for (size_t i = 0; i < count; i++) {
    xml[pos++] = 'a';
    xml[pos++] = 0;
  }
  xml[pos++] = 0x3d;
  xml[pos++] = (char)0xd8;
  xml[pos++] = 0x00;
  xml[pos++] = (char)0xde;
  for (const char *p = end; *p; p++) {
    xml[pos++] = *p;
    xml[pos++] = 0;
  }

  parser = XML_ParserCreate(NULL);
  if (! parser) {
    free(xml);
    return 2;
  }
  status = XML_Parse(parser, xml, (int)pos, XML_TRUE);
  if (status != XML_STATUS_OK)
    fprintf(stderr, "parse error %d\n", XML_GetErrorCode(parser));
  XML_ParserFree(parser);
  free(xml);
  return status == XML_STATUS_OK ? 0 : 1;
}
