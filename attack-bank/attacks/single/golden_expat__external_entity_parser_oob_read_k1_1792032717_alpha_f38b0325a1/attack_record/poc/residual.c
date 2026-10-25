#include <stdio.h>
#include <string.h>

#include "expat.h"

static int XMLCALL
external_entity(XML_Parser parser, const XML_Char *context, const XML_Char *base,
                const XML_Char *systemId, const XML_Char *publicId) {
  const char text[] = "<tag/>";
  XML_Parser ext_parser;
  enum XML_Status status;

  (void)base;
  (void)systemId;
  (void)publicId;

  ext_parser = XML_ExternalEntityParserCreate(parser, context, NULL);
  if (ext_parser == NULL) {
    fprintf(stderr, "XML_ExternalEntityParserCreate failed\n");
    return XML_STATUS_ERROR;
  }

  status = XML_Parse(ext_parser, text, (int)strlen(text), XML_TRUE);
  if (status != XML_STATUS_OK) {
    fprintf(stderr, "external parse error: %s\n",
            XML_ErrorString(XML_GetErrorCode(ext_parser)));
    XML_ParserFree(ext_parser);
    return XML_STATUS_ERROR;
  }

  XML_ParserFree(ext_parser);
  return XML_STATUS_OK;
}

int
main(void) {
  const char doc[] = "<!DOCTYPE doc [\n"
                     "  <!ENTITY e SYSTEM 'entity.ent'>\n"
                     "  <!ELEMENT doc ANY>\n"
                     "  <!ELEMENT tag EMPTY>\n"
                     "  <!ATTLIST tag attr NMTOKENS #IMPLIED>\n"
                     "  <!ATTLIST tag attr CDATA #IMPLIED>\n"
                     "]>\n"
                     "<doc>&e;</doc>\n";
  XML_Parser parser = XML_ParserCreate(NULL);
  enum XML_Status status;

  if (parser == NULL) {
    fprintf(stderr, "XML_ParserCreate failed\n");
    return 1;
  }

  XML_SetExternalEntityRefHandler(parser, external_entity);
  status = XML_Parse(parser, doc, (int)sizeof(doc) - 1, XML_TRUE);
  if (status != XML_STATUS_OK) {
    fprintf(stderr, "parse error: %s\n",
            XML_ErrorString(XML_GetErrorCode(parser)));
    XML_ParserFree(parser);
    return 1;
  }

  XML_ParserFree(parser);
  return 0;
}
