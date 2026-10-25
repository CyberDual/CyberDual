#include <stdio.h>
#include <string.h>
#include "expat.h"

static int XMLCALL
ext_handler(XML_Parser parser, const XML_Char *context, const XML_Char *base,
            const XML_Char *systemId, const XML_Char *publicId) {
  const char *text = "<tag second=' a  b '/>";
  (void)base; (void)systemId; (void)publicId;
  XML_Parser ext = XML_ExternalEntityParserCreate(parser, context, NULL);
  if (ext == NULL) { fprintf(stderr, "no ext parser\n"); return XML_STATUS_ERROR; }
  if (XML_Parse(ext, text, (int)strlen(text), XML_TRUE) != XML_STATUS_OK)
    fprintf(stderr, "ext parse error\n");
  XML_ParserFree(ext);
  return XML_STATUS_OK;
}

int main(void) {
  const char *text =
      "<!DOCTYPE doc [\n"
      "  <!ENTITY e SYSTEM 'entity.ent'>\n"
      "  <!ELEMENT doc ANY>\n"
      "  <!ELEMENT tag EMPTY>\n"
      "  <!ATTLIST tag first CDATA #IMPLIED>\n"
      "  <!ATTLIST tag second NMTOKENS #IMPLIED>\n"
      "]>\n"
      "<doc>&e;</doc>\n";
  XML_Parser parser = XML_ParserCreate(NULL);
  XML_SetExternalEntityRefHandler(parser, ext_handler);
  if (XML_Parse(parser, text, (int)strlen(text), XML_TRUE) != XML_STATUS_OK)
    fprintf(stderr, "parse error: %s\n", XML_ErrorString(XML_GetErrorCode(parser)));
  XML_ParserFree(parser);
  printf("done\n");
  return 0;
}
