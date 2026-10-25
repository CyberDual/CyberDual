#include <expat.h>
#include <stdio.h>
#include <string.h>

static int XMLCALL
external_entity_loader(XML_Parser parser, const XML_Char *context,
                       const XML_Char *base, const XML_Char *systemId,
                       const XML_Char *publicId) {
  const char *const document = "<tag second=' a  b '/>";
  XML_Parser external_parser;
  enum XML_Status status;
  (void)base;
  (void)systemId;
  (void)publicId;

  external_parser = XML_ExternalEntityParserCreate(parser, context, NULL);
  if (external_parser == NULL)
    return XML_STATUS_ERROR;

  status = XML_Parse(external_parser, document, (int)strlen(document), XML_TRUE);
  XML_ParserFree(external_parser);
  return status;
}

int main(void) {
  const char *const document =
      "<!DOCTYPE doc [\n"
      "  <!ENTITY entity SYSTEM 'entity.xml'>\n"
      "  <!ELEMENT doc ANY>\n"
      "  <!ELEMENT tag EMPTY>\n"
      "  <!ATTLIST tag first CDATA #IMPLIED>\n"
      "  <!ATTLIST tag second NMTOKENS #IMPLIED>\n"
      "]>\n"
      "<doc>&entity;</doc>\n";
  XML_Parser parser = XML_ParserCreate(NULL);
  enum XML_Status status;

  if (parser == NULL)
    return 2;
  XML_SetExternalEntityRefHandler(parser, external_entity_loader);
  status = XML_Parse(parser, document, (int)strlen(document), XML_TRUE);
  XML_ParserFree(parser);
  return status == XML_STATUS_OK ? 0 : 1;
}
