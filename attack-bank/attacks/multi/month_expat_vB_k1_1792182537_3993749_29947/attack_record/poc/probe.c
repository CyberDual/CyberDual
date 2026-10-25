/* PoC: double-free of the unknown-encoding "data" pointer.
 *
 * Expat lets an application resolve an otherwise-unknown document encoding
 * via XML_SetUnknownEncodingHandler().  The handler hands Expat a heap buffer
 * (info.data) together with a release callback (info.release); Expat promises
 * to invoke that release callback exactly once, when the encoding is no longer
 * needed (at XML_ParserReset()/XML_ParserFree() teardown).
 *
 * On the resulting tree the release callback is invoked twice for a single
 * (data, release) pair -- once from XML_ParserReset() and again from
 * XML_ParserFree() -- because neither teardown path clears the stored hook
 * afterwards.  The second invocation frees an already-freed pointer.
 *
 * Everything below uses only the public API with in-contract input.
 */
#include <expat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void XMLCALL
release_encoding_data(void *data) {
  free(data); /* frees the buffer Expat was handed in info.data */
}

static int XMLCALL
unknown_encoding_handler(void *userData, const XML_Char *name,
                         XML_Encoding *info) {
  (void)userData;
  (void)name;
  for (int i = 0; i < 256; i++)
    info->map[i] = i; /* valid single-byte encoding */
  info->data = malloc(64); /* owned by Expat; released via the callback */
  info->convert = NULL;
  info->release = release_encoding_data;
  return XML_STATUS_OK;
}

int
main(void) {
  const char *const doc
      = "<?xml version='1.0' encoding='application-defined'?><r>ok</r>";

  XML_Parser parser = XML_ParserCreate(NULL);
  if (parser == NULL)
    return 2;
  XML_SetUnknownEncodingHandler(parser, unknown_encoding_handler, NULL);

  if (XML_Parse(parser, doc, (int)strlen(doc), /*isFinal=*/1)
      == XML_STATUS_ERROR) {
    /* Parsing is expected to succeed with the application-defined encoding. */
    fprintf(stderr, "parse error: %s\n",
            XML_ErrorString(XML_GetErrorCode(parser)));
  }

  XML_ParserReset(parser, NULL); /* teardown #1: release runs here */
  XML_ParserFree(parser);        /* teardown #2: release must NOT run again */

  printf("completed without fault\n");
  return 0;
}
