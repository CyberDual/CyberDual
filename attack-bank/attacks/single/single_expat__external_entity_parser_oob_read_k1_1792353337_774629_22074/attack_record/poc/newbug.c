#include <stdio.h>
#include <string.h>
#include <expat.h>

static int XMLCALL
extEntityRefHandler(XML_Parser parser, const XML_Char *context,
                    const XML_Char *base, const XML_Char *systemId,
                    const XML_Char *publicId) {
  (void)base;
  (void)systemId;
  (void)publicId;
  XML_Parser extParser = XML_ExternalEntityParserCreate(parser, context, NULL);
  if (! extParser)
    return XML_STATUS_ERROR;
  const char extContent[] = "<e/>";
  if (XML_Parse(extParser, extContent, (int)strlen(extContent), XML_TRUE)
      == XML_STATUS_ERROR)
    fprintf(stderr, "external entity parse error: %s\n",
            XML_ErrorString(XML_GetErrorCode(extParser)));
  XML_ParserFree(extParser);
  return XML_STATUS_OK;
}

int
main(void) {
  const char doc[]
      = "<!DOCTYPE r [\n"
        "<!ELEMENT r ANY>\n"
        "<!ELEMENT e EMPTY>\n"
        "<!ATTLIST e p:a1 CDATA #IMPLIED p:a2 CDATA #IMPLIED p:a3 CDATA #IMPLIED\n"
        "            p:a4 CDATA #IMPLIED p:a5 CDATA #IMPLIED p:a6 CDATA #IMPLIED\n"
        "            p:a7 CDATA #IMPLIED p:a8 CDATA #IMPLIED p:a9 CDATA #IMPLIED>\n"
        "<!ENTITY xxe SYSTEM \"ext.ent\">\n"
        "]>\n"
        "<r>&xxe;</r>";
  XML_Parser parser = XML_ParserCreateNS(NULL, '\n');
  if (! parser)
    return 1;
  XML_SetParamEntityParsing(parser, XML_PARAM_ENTITY_PARSING_ALWAYS);
  XML_SetExternalEntityRefHandler(parser, extEntityRefHandler);
  if (XML_Parse(parser, doc, (int)strlen(doc), XML_TRUE) == XML_STATUS_ERROR)
    fprintf(stderr, "parse error: %s\n",
            XML_ErrorString(XML_GetErrorCode(parser)));
  XML_ParserFree(parser);
  return 0;
}
