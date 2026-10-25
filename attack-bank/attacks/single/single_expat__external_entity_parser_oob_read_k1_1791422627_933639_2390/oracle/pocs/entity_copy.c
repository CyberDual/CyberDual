#include <stdio.h>
#include <string.h>
#include "expat.h"

static int XMLCALL
parse_entity(XML_Parser parent, const XML_Char *context,
             const XML_Char *base, const XML_Char *systemId,
             const XML_Char *publicId) {
  XML_Parser child;
  const char entity[] = "<item/>";
  int ok;

  (void)base;
  (void)systemId;
  (void)publicId;
  child = XML_ExternalEntityParserCreate(parent, context, NULL);
  if (! child)
    return XML_STATUS_ERROR;
  ok = XML_Parse(child, entity, (int)strlen(entity), XML_TRUE)
       == XML_STATUS_OK;
  XML_ParserFree(child);
  return ok;
}

int
main(void) {
  const char document[]
      = "<!DOCTYPE root ["
        "<!ATTLIST item flag CDATA #IMPLIED>"
        "<!ATTLIST item flag CDATA #IMPLIED>"
        "<!ENTITY ext SYSTEM 'ext'>"
        "]><root>&ext;</root>";
  XML_Parser parser = XML_ParserCreate(NULL);
  int ok;

  if (! parser)
    return 2;
  XML_SetExternalEntityRefHandler(parser, parse_entity);
  ok = XML_Parse(parser, document, (int)strlen(document), XML_TRUE)
       == XML_STATUS_OK;
  if (! ok)
    fprintf(stderr, "%s\n", XML_ErrorString(XML_GetErrorCode(parser)));
  XML_ParserFree(parser);
  return ok ? 0 : 1;
}
