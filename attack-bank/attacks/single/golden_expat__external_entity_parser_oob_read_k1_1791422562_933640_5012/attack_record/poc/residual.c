#include <expat.h>
#include <stdio.h>
#include <string.h>

static int XMLCALL
parse_external(XML_Parser parent, const XML_Char *context,
               const XML_Char *base, const XML_Char *systemId,
               const XML_Char *publicId) {
  static const char entity[] = "<tag second=' a  b '/>";
  XML_Parser child;
  enum XML_Status result;
  (void)base;
  (void)systemId;
  (void)publicId;
  child = XML_ExternalEntityParserCreate(parent, context, NULL);
  if (!child)
    return XML_STATUS_ERROR;
  result = XML_Parse(child, entity, (int)strlen(entity), XML_TRUE);
  if (result != XML_STATUS_OK)
    fprintf(stderr, "external parse: %s\n", XML_ErrorString(XML_GetErrorCode(child)));
  XML_ParserFree(child);
  return result;
}

int
main(void) {
  static const char document[] =
      "<!DOCTYPE doc ["
      "<!ENTITY e SYSTEM 'entity.ent'>"
      "<!ELEMENT doc ANY>"
      "<!ELEMENT tag EMPTY>"
      "<!ATTLIST tag first CDATA 'x'>"
      "<!ATTLIST tag second NMTOKENS #IMPLIED>"
      "]><doc>&e;</doc>";
  XML_Parser parser = XML_ParserCreate(NULL);
  enum XML_Status result;
  if (!parser)
    return 2;
  XML_SetExternalEntityRefHandler(parser, parse_external);
  result = XML_Parse(parser, document, (int)strlen(document), XML_TRUE);
  if (result != XML_STATUS_OK)
    fprintf(stderr, "document parse: %s\n", XML_ErrorString(XML_GetErrorCode(parser)));
  XML_ParserFree(parser);
  return result == XML_STATUS_OK ? 0 : 1;
}
