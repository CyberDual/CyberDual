/* Drives XML_ExternalEntityParserCreate over a document whose DTD declares
 * a tokenized (non-CDATA) attribute for an element; the external entity
 * then uses that element with the attribute specified, so storeAtts has to
 * resolve the attribute's declaration through the DTD copy made by dtdCopy.
 */
#include <expat.h>

#include <stdio.h>
#include <string.h>

static int XMLCALL
external_entity_ref_handler(XML_Parser parser, const XML_Char *context,
                            const XML_Char *base, const XML_Char *systemId,
                            const XML_Char *publicId) {
  const char *const text = "<tag a=' x  y '/>";
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
  if (status == XML_STATUS_ERROR)
    fprintf(stderr, "entity parse error: %s\n",
            XML_ErrorString(XML_GetErrorCode(ext_parser)));
  XML_ParserFree(ext_parser);
  return status == XML_STATUS_OK ? XML_STATUS_OK : XML_STATUS_ERROR;
}

int
main(void) {
  const char *const doc = "<!DOCTYPE doc [\n"
                          "  <!ELEMENT doc ANY>\n"
                          "  <!ELEMENT tag EMPTY>\n"
                          "  <!ATTLIST tag a NMTOKENS #IMPLIED>\n"
                          "  <!ENTITY e SYSTEM 'entity.ent'>\n"
                          "]>\n"
                          "<doc>&e;</doc>\n";
  XML_Parser parser = XML_ParserCreate(NULL);
  if (parser == NULL) {
    fprintf(stderr, "XML_ParserCreate failed\n");
    return 2;
  }
  XML_SetExternalEntityRefHandler(parser, external_entity_ref_handler);
  if (XML_Parse(parser, doc, (int)strlen(doc), XML_TRUE) == XML_STATUS_ERROR) {
    fprintf(stderr, "parse error: %s\n",
            XML_ErrorString(XML_GetErrorCode(parser)));
    XML_ParserFree(parser);
    return 1;
  }
  XML_ParserFree(parser);
  printf("OK\n");
  return 0;
}
