#include <stdio.h>
#include <string.h>

#include "expat.h"

static const char ext_text[] = "<e xmlns:p='http://example.org/'/>";

static int XMLCALL
handle_external(XML_Parser parser, const XML_Char *context, const XML_Char *base,
                const XML_Char *systemId, const XML_Char *publicId) {
  XML_Parser ext;
  enum XML_Status status;

  (void)base;
  (void)systemId;
  (void)publicId;

  ext = XML_ExternalEntityParserCreate(parser, context, NULL);
  if (ext == NULL)
    return XML_STATUS_ERROR;
  status = XML_Parse(ext, ext_text, (int)strlen(ext_text), XML_TRUE);
  XML_ParserFree(ext);
  return status == XML_STATUS_OK ? XML_STATUS_OK : XML_STATUS_ERROR;
}

int
main(void) {
  const char doc[] = "<!DOCTYPE doc [\n"
                     "  <!ELEMENT doc ANY>\n"
                     "  <!ELEMENT e EMPTY>\n"
                     "  <!ATTLIST e p:a CDATA 'v'>\n"
                     "  <!ENTITY en SYSTEM 'ent'>\n"
                     "]>\n"
                     "<doc>&en;</doc>\n";
  XML_Parser parser;
  enum XML_Status status;

  parser = XML_ParserCreateNS(NULL, ' ');
  if (parser == NULL)
    return 1;
  XML_SetExternalEntityRefHandler(parser, handle_external);
  status = XML_Parse(parser, doc, (int)strlen(doc), XML_TRUE);
  XML_ParserFree(parser);
  return status == XML_STATUS_OK ? 0 : 1;
}
