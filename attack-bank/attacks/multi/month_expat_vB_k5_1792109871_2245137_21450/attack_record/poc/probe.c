/* Drives the expat public API: registers an unknown-encoding handler whose
 * release callback queries the current parse position (a supported pattern;
 * the release callback may access the parser), then frees the parser.
 *
 * On a healthy tree this prints the release-callback observations and exits
 * 0.  On the defective tree the release callback runs after the parse buffer
 * has already been freed, so XML_GetCurrentLineNumber64() walks freed heap
 * memory (XmlUpdatePosition over [m_positionPtr, m_eventPtr)) and the
 * AddressSanitizer build aborts.
 */

#include <stdio.h>
#include <string.h>

#include "expat.h"

typedef struct {
  XML_Parser parser;
  int releaseCallCount;
  unsigned long lineAtRelease;
} ReleaseData;

static void XMLCALL
encodingRelease(void *userData) {
  ReleaseData *const data = (ReleaseData *)userData;
  data->releaseCallCount++;
  /* Supported: access the parser from the release callback. */
  data->lineAtRelease = (unsigned long)XML_GetCurrentLineNumber64(data->parser);
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
  const char *const doc = "<?xml version='1.0' encoding='release-order'?>\n"
                          "<root>\n"
                          "  <unclosed>\n"
                          "</root>\n";
  XML_Parser parser = XML_ParserCreate(NULL);
  ReleaseData data;

  if (parser == NULL) {
    fprintf(stderr, "XML_ParserCreate failed\n");
    return 2;
  }

  data.parser = parser;
  data.releaseCallCount = 0;
  data.lineAtRelease = 0;

  XML_SetUnknownEncodingHandler(parser, unknownEncodingHandler, &data);

  /* Expected to fail with XML_ERROR_TAG_MISMATCH; either way the unknown
     encoding stays installed and its release callback fires at free time. */
  (void)XML_Parse(parser, doc, (int)strlen(doc), XML_TRUE);

  XML_ParserFree(parser);

  printf("releaseCallCount=%d lineAtRelease=%lu\n", data.releaseCallCount,
         data.lineAtRelease);
  return 0;
}
