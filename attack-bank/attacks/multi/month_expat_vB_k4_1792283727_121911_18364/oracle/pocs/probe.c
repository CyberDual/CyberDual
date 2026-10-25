/* Expat: parser access from the unknown-encoding release callback.
 *
 * The library contract (covered by expat's own test suite,
 * test_misc_unknown_encoding_release_getter_parser_reset) is that the
 * application-defined release callback may call parser getters such as
 * XML_GetCurrentLineNumber64() while the parser is being reset or freed.
 *
 * This probe registers an unknown encoding, parses a document that makes
 * the parser adopt it, then resets the parser.  During XML_ParserReset
 * the release callback runs and asks for the current line number, which
 * makes the library scan the parser's input buffer.
 */
#include <stdio.h>
#include <string.h>

#include "expat.h"

typedef struct {
  XML_Parser parser;
  int releaseCallCount;
  uint64_t lineAtRelease;
} EncodingReleaseData;

static void XMLCALL
position_reading_encoding_release(void *userData) {
  EncodingReleaseData *const data = userData;
  data->releaseCallCount++;
  data->lineAtRelease = XML_GetCurrentLineNumber64(data->parser);
}

static int XMLCALL
position_reading_encoding_handler(void *userData, const XML_Char *name,
                                  XML_Encoding *info) {
  (void)name;

  for (int i = 0; i < 256; i++)
    info->map[i] = i;
  info->data = userData;
  info->convert = NULL;
  info->release = position_reading_encoding_release;
  return XML_STATUS_OK;
}

int main(void) {
  const char *const doc = "<?xml version='1.0' encoding='release-order'?>\n"
                          "<root>\n"
                          "  <unclosed>\n"
                          "</root>\n";
  XML_Parser parser = XML_ParserCreate(NULL);
  EncodingReleaseData data = {parser, 0, 0};

  if (parser == NULL) {
    fprintf(stderr, "XML_ParserCreate failed\n");
    return 1;
  }

  XML_SetUnknownEncodingHandler(parser, position_reading_encoding_handler,
                                &data);

  /* Fails with XML_ERROR_TAG_MISMATCH after the encoding was adopted. */
  if (XML_Parse(parser, doc, (int)strlen(doc), XML_TRUE) == XML_STATUS_OK) {
    fprintf(stderr, "parse unexpectedly succeeded\n");
    return 1;
  }

  XML_ParserReset(parser, NULL);
  XML_ParserFree(parser);

  printf("release calls: %d, line at release: %llu\n", data.releaseCallCount,
         (unsigned long long)data.lineAtRelease);
  return 0;
}
