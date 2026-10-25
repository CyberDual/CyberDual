/*
 * Regression test case for UTF-16 documents with large internal entities
 * referenced from an attribute value.
 *
 * Build (from the top of the source tree, with an XML_UNICODE build of
 * libexpat available as an AddressSanitizer-instrumented static library):
 *
 *   cc -fsanitize=address -g -O1 -DXML_UNICODE \
 *       -I<build-dir-with-expat_config.h> -Iexpat/lib \
 *       pocs/newbug.c <build-dir>/libexpatw.a -o newbug && ./newbug
 *
 * The document below is well-formed XML 1.0: an internal general entity
 * whose replacement text is 2048 characters, referenced from an attribute
 * value.  It is handed to XML_Parse() in small pieces, the way a streaming
 * consumer would.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expat_config.h"
#include "expat.h"

/* Replacement text of the internal entity, in UTF-16 code units. */
#define ENTITY_TEXT_LEN 2048

static unsigned short doc[8192];
static size_t docLen = 0;

static void
putAscii(const char *s) {
  while (*s)
    doc[docLen++] = (unsigned char)*s++;
}

static void
buildDocument(void) {
  doc[docLen++] = 0xFEFF; /* byte order mark */
  putAscii("<?xml version='1.0' encoding='UTF-16'?>\n");
  putAscii("<!DOCTYPE d [\n<!ENTITY e \"");
  for (int i = 0; i < ENTITY_TEXT_LEN; i++)
    doc[docLen++] = 'A';
  putAscii("\">\n]>\n");
  putAscii("<d a='&e;'/>\n");
}

static void XMLCALL
startElement(void *userData, const XML_Char *name, const XML_Char **atts) {
  (void)userData;
  (void)name;
  (void)atts;
}

static void XMLCALL
endElement(void *userData, const XML_Char *name) {
  (void)userData;
  (void)name;
}

int
main(int argc, char **argv) {
  /* Bytes per XML_Parse() call; 0 hands over the whole document at once. */
  const size_t chunk = (argc > 1) ? (size_t)atoi(argv[1]) : 0;

  buildDocument();
  const char *const bytes = (const char *)doc;
  const size_t bytesLen = docLen * sizeof(unsigned short);

  XML_Parser parser = XML_ParserCreate(NULL);
  if (parser == NULL) {
    fprintf(stderr, "Failed to create parser\n");
    return 1;
  }
  XML_SetElementHandler(parser, startElement, endElement);

  enum XML_Status status = XML_STATUS_ERROR;
  if (chunk == 0) {
    status = XML_Parse(parser, bytes, (int)bytesLen, 1);
  } else {
    size_t off = 0;
    while (off < bytesLen) {
      const size_t take
          = (bytesLen - off > chunk) ? chunk : (bytesLen - off);
      status = XML_Parse(parser, bytes + off, (int)take,
                         (off + take == bytesLen));
      if (status == XML_STATUS_ERROR)
        break;
      off += take;
    }
  }

  if (status == XML_STATUS_ERROR)
    fprintf(stderr, "Expat error: %s (line %lu)\n",
            XML_ErrorString(XML_GetErrorCode(parser)),
            (unsigned long)XML_GetCurrentLineNumber(parser));
  else
    fprintf(stderr, "Document parsed successfully\n");

  XML_ParserFree(parser);
  return 0;
}
