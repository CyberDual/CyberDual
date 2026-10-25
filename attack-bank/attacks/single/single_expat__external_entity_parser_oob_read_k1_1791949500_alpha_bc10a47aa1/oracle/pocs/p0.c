#include <expat.h>
#include <stdio.h>
#include <string.h>

static int XMLCALL
external_entity_loader(XML_Parser parser, const XML_Char *context,
                       const XML_Char *base, const XML_Char *systemId,
                       const XML_Char *publicId) {
  XML_Parser ext;
  const char *doc = "<e a=' x '/>";

  (void)base;
  (void)systemId;
  (void)publicId;

  ext = XML_ExternalEntityParserCreate(parser, context, NULL);
  if (ext == NULL)
    return XML_STATUS_ERROR;

  if (XML_Parse(ext, doc, (int)strlen(doc), XML_TRUE) == XML_STATUS_ERROR) {
    XML_ParserFree(ext);
    return XML_STATUS_ERROR;
  }
  XML_ParserFree(ext);
  return XML_STATUS_OK;
}

int
main(void) {
  const char *text = "<!DOCTYPE d [\n"
                     "  <!ELEMENT d ANY>\n"
                     "  <!ELEMENT e EMPTY>\n"
                     "  <!ATTLIST e a NMTOKEN 'def'>\n"
                     "  <!ENTITY x SYSTEM 'x'>\n"
                     "]>\n"
                     "<d>&x;</d>";
  XML_Parser parser = XML_ParserCreate(NULL);
  int ret = 0;

  if (parser == NULL)
    return 1;

  XML_SetExternalEntityRefHandler(parser, external_entity_loader);
  if (XML_Parse(parser, text, (int)strlen(text), XML_TRUE)
      == XML_STATUS_ERROR) {
    fprintf(stderr, "Parse error: %s\n",
            XML_ErrorString(XML_GetErrorCode(parser)));
    ret = 1;
  }
  XML_ParserFree(parser);
  return ret;
}
