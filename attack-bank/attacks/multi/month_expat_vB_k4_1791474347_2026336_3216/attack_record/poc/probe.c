/* Proof-of-concept for the benchmark harness.
 *
 * Drives libexpat's public API only (expat.h). It registers an unknown
 * encoding handler whose release callback reads the parser's current
 * position through the public accessor XML_GetCurrentLineNumber64(),
 * then frees the parser with XML_ParserFree().
 */

#include <expat.h>
#include <stdint.h>
#include <string.h>

typedef struct {
  XML_Parser parser;
  uint64_t lineAtRelease;
} ReleaseCtx;

static void XMLCALL
release_cb(void *userData) {
  ReleaseCtx *const ctx = (ReleaseCtx *)userData;
  /* Reads parser->m_eventPtr / parser->m_encoding under the hood. */
  ctx->lineAtRelease = XML_GetCurrentLineNumber64(ctx->parser);
}

static int XMLCALL
unknown_encoding_handler(void *userData, const XML_Char *name,
                         XML_Encoding *info) {
  (void)name;
  for (int i = 0; i < 256; i++)
    info->map[i] = i;
  info->data = userData;
  info->convert = NULL;
  info->release = release_cb;
  return XML_STATUS_OK;
}

int
main(void) {
  const char *const doc = "<?xml version='1.0' encoding='release-order'?>\n"
                          "<root>\n"
                          "  <unclosed>\n"
                          "</root>\n";

  XML_Parser parser = XML_ParserCreate(NULL);
  if (!parser)
    return 1;

  ReleaseCtx ctx;
  ctx.parser = parser;
  ctx.lineAtRelease = 0;

  XML_SetUnknownEncodingHandler(parser, unknown_encoding_handler, &ctx);

  /* Malformed on purpose (unclosed <unclosed> tag) so that m_eventPtr ends
   * up pointing into the live parse buffer at the point of error, which is
   * exactly what the release callback below will read back out. */
  XML_Parse(parser, doc, (int)strlen(doc), 1);

  /* Tears the parser down. The release callback above fires during
   * teardown and reads parser state through the public API. */
  XML_ParserFree(parser);

  return 0;
}
