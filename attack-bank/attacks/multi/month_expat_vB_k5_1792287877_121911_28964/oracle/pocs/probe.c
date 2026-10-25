/* Drive expat's public API through a suspended/resumed parse.
 *
 * The processing-instruction handler suspends the parser mid-document;
 * the application then resumes the parse and asks XML_GetBuffer for far
 * more room than the current buffer has left, which makes expat
 * re-allocate the internal buffer and free the old allocation.
 *
 * XML_DefaultCurrent, called between chunks while the parse is in
 * progress, must re-report the current event safely.
 */
#include <string.h>
#include <expat.h>

static XML_Parser parser;

static void
pi_handler(void *userData, const XML_Char *target, const XML_Char *data) {
  /* nothing to report; we only suspend (resumable) mid-document */
  (void)userData;
  (void)target;
  (void)data;
  XML_StopParser(parser, XML_TRUE);
}

static void
default_handler(void *userData, const XML_Char *s, int len) {
  /* nothing to do; we only need the handler to be installed */
  (void)userData;
  (void)s;
  (void)len;
}

int
main(void) {
  const char *const doc = "<?pi x?><r/>";

  parser = XML_ParserCreate("ISO-8859-1");
  if (parser == NULL)
    return 1;

  XML_SetProcessingInstructionHandler(parser, pi_handler);
  XML_SetDefaultHandler(parser, default_handler);

  /* First, non-final chunk: the PI handler suspends the parser. */
  if (XML_Parse(parser, doc, (int)strlen(doc), XML_FALSE) != XML_STATUS_SUSPENDED)
    return 2;

  /* Continue the parse; the parser is left mid-document. */
  if (XML_ResumeParser(parser) != XML_STATUS_OK)
    return 3;

  /* Requesting 1 MiB forces the buffer to be re-allocated and the
     previous allocation to be freed. */
  if (XML_GetBuffer(parser, 1 << 20) == NULL)
    return 4;

  /* Re-report the current event while the parse is in progress. */
  XML_DefaultCurrent(parser);

  XML_ParserFree(parser);
  return 0;
}
