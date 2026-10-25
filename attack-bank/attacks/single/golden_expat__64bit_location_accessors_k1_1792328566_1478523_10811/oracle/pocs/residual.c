/* Drives the deprecated-but-supported public API XML_GetInputContext()
   from inside a start-element handler during an active parse, exactly as
   documented, with ordinary well-formed input. */
#include <stdio.h>
#include <string.h>

#include <expat.h>

static void XMLCALL
start_element(void *userData, const XML_Char *name, const XML_Char **atts) {
  XML_Parser parser = (XML_Parser)userData;
  int offset = -1;
  int size = -1;
  const char *buffer;

  (void)name;
  (void)atts;

  buffer = XML_GetInputContext(parser, &offset, &size);
  if (buffer == NULL) {
    fprintf(stderr, "no input context available\n");
    return;
  }
  printf("context: offset=%d size=%d\n", offset, size);
}

int
main(void) {
  const char *doc = "<root><child>text</child></root>";
  XML_Parser parser = XML_ParserCreate(NULL);
  if (parser == NULL) {
    fprintf(stderr, "XML_ParserCreate failed\n");
    return 1;
  }
  XML_SetUserData(parser, parser);
  XML_SetStartElementHandler(parser, start_element);
  if (XML_Parse(parser, doc, (int)strlen(doc), XML_TRUE) == XML_STATUS_ERROR) {
    fprintf(stderr, "parse error: %s\n",
            XML_ErrorString(XML_GetErrorCode(parser)));
    XML_ParserFree(parser);
    return 1;
  }
  XML_ParserFree(parser);
  return 0;
}
