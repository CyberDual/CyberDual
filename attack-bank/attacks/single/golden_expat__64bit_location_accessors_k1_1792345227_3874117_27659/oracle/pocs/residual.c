/*
 * Residual defect PoC -- libexpat 64bit location information
 *
 * Extracts the raw bytes of the current event the way the Expat test
 * suite does (see dup_original_string() in tests/misc_tests.c, inspired
 * by XML::Parser's XML_OriginalString): XML_GetInputContext() reports
 * the input buffer plus the offset of the current parse position, and
 * XML_GetCurrentByteCount() reports the length of the current event.
 *
 * The document is a plain, well-formed XML fragment fed to XML_Parse()
 * in one fixed-size chunk, as usual for streamed input.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <expat.h>

#define CHUNK_LEN 2048

static XML_Parser g_parser;

static void XMLCALL
char_handler(void *userData, const XML_Char *s, int len) {
  (void)userData;
  (void)s;
  (void)len;

  int offset = -1;
  int size = -1;
  const char *const context = XML_GetInputContext(g_parser, &offset, &size);
  const int byte_count = XML_GetCurrentByteCount(g_parser);
  char *raw;
  if (context == NULL || byte_count <= 0)
    return;

  fprintf(stderr, "event: offset=%d size=%d byte_count=%d\n", offset, size,
          byte_count);

  /* offset is the position of the event within the reported buffer;
     duplicate the raw bytes of the event from there, the way
     dup_original_string() in the Expat test suite does */
  raw = strndup(context + offset, (size_t)byte_count);
  if (raw == NULL)
    exit(2);
  fprintf(stderr, "event starts with %.4s\n", raw);
  free(raw);
}

int
main(void) {
  char *const doc = (char *)malloc(CHUNK_LEN);
  XML_Parser parser;

  if (doc == NULL)
    return 2;

  /* <doc> + 2000 bytes of character data + <x/> + pending tail */
  memset(doc, 'a', CHUNK_LEN);
  memcpy(doc, "<doc>", 5);
  memcpy(doc + 5 + 2000, "<x/>", 4);

  parser = XML_ParserCreate(NULL);
  if (parser == NULL) {
    free(doc);
    return 2;
  }
  g_parser = parser;
  XML_SetCharacterDataHandler(parser, char_handler);

  if (XML_Parse(parser, doc, CHUNK_LEN, /*isFinal=*/XML_FALSE)
      == XML_STATUS_ERROR) {
    fprintf(stderr, "parse error: %s\n", XML_ErrorString(XML_GetErrorCode(parser)));
    XML_ParserFree(parser);
    free(doc);
    return 1;
  }

  XML_ParserFree(parser);
  free(doc);
  fprintf(stderr, "done\n");
  return 0;
}
