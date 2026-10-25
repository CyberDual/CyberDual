/* Drives the unknown-encoding release callback contract: the release
   callback is documented to be allowed to access the parser, e.g. to
   query the final parse position, while the parser is being torn down
   or reset.

   The document below uses an unknown single-byte encoding and stops
   with a mismatched-tag error, leaving the encoding release callback
   pending.  XML_ParserReset then runs the callback, which asks the
   parser for the position where parsing stopped. */

#include <expat.h>

#include <stdio.h>
#include <string.h>

typedef struct {
  XML_Parser parser;
  int releaseCallCount;
  unsigned long lineAtRelease;
} EncodingReleaseData;

static void XMLCALL
encodingRelease(void *userData) {
  EncodingReleaseData *const data = (EncodingReleaseData *)userData;
  data->releaseCallCount++;
  /* Access the parser from inside the release callback: query the
     position where parsing stopped. */
  data->lineAtRelease
      = (unsigned long)XML_GetCurrentLineNumber64(data->parser);
}

static int XMLCALL
unknownEncodingHandler(void *userData, const XML_Char *name,
                       XML_Encoding *info) {
  int i;
  (void)name;
  for (i = 0; i < 256; i++)
    info->map[i] = i;
  info->data = userData;
  info->convert = NULL;
  info->release = encodingRelease;
  return XML_STATUS_OK;
}

int
main(void) {
  const char *const doc = "<?xml version='1.0' encoding='x-weird'?>\n"
                          "<root>\n"
                          "  <unclosed>\n"
                          "</root>\n";
  XML_Parser parser = XML_ParserCreate(NULL);
  EncodingReleaseData data = {parser, 0, 0};

  if (! parser)
    return 2;

  XML_SetUnknownEncodingHandler(parser, unknownEncodingHandler, &data);

  /* Parse fails (mismatched tag) with the unknown encoding installed,
     so the release callback stays pending. */
  if (XML_Parse(parser, doc, (int)strlen(doc), XML_TRUE) == XML_STATUS_OK)
    printf("parse unexpectedly ok\n");

  /* Resetting the parser invokes the pending release callback. */
  XML_ParserReset(parser, NULL);
  XML_ParserFree(parser);

  printf("releaseCallCount=%d lineAtRelease=%lu\n", data.releaseCallCount,
         data.lineAtRelease);
  return 0;
}
