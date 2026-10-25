#include <stdio.h>
#include <string.h>
#include "expat.h"

static XML_Parser g_parser;

static void XMLCALL my_release(void *data) {
  (void)data;
  /* Reading back the parse position from inside the release callback is a
     documented, supported use of the parser handle. */
  unsigned long long line = XML_GetCurrentLineNumber64(g_parser);
  fprintf(stderr, "release: line=%llu\n", line);
}

static int XMLCALL my_unknown(void *data, const XML_Char *name,
                              XML_Encoding *info) {
  (void)data;
  (void)name;
  for (int i = 0; i < 256; i++)
    info->map[i] = i;
  info->data = data;
  info->convert = NULL;
  info->release = my_release;
  return XML_STATUS_OK;
}

int main(void) {
  const char *doc
      = "<?xml version='1.0' encoding='weird'?>\n<root>\n  <unclosed>\n</root>\n";
  g_parser = XML_ParserCreate(NULL);
  XML_SetUnknownEncodingHandler(g_parser, my_unknown, NULL);
  if (XML_Parse(g_parser, doc, (int)strlen(doc), XML_TRUE) == XML_STATUS_ERROR)
    fprintf(stderr, "parse error: %s\n",
            XML_ErrorString(XML_GetErrorCode(g_parser)));
  XML_ParserReset(g_parser, NULL);
  fprintf(stderr, "reset done\n");
  XML_ParserFree(g_parser);
  fprintf(stderr, "done\n");
  return 0;
}
