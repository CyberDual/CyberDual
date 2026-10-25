/* PoC: NULL parser handling of XML_GetInputContext64 (Expat 2.9.0+)
 *
 * The 64bit location accessors were added in Expat 2.9.0.  This program
 * binds XML_GetInputContext64 as a weak symbol so that the very same
 * binary also runs against older versions of Expat that do not have the
 * function yet; there it simply skips the new API and exits cleanly.
 *
 * On Expat 2.9.0+ the call with a NULL parser crashes inside the library:
 * unlike its 32bit sibling XML_GetInputContext (and unlike every other
 * location accessor), XML_GetInputContext64 dereferences the parser
 * pointer before validating it.
 */
#include <expat.h>
#include <string.h>

#if defined(__GNUC__)
#  define HAVE_XML_GET_INPUT_CONTEXT64 1
extern const char *XML_GetInputContext64(XML_Parser parser, long long *offset,
                                         long long *size)
    __attribute__((weak));
#endif

static int g_ok = 1;

#if defined(HAVE_XML_GET_INPUT_CONTEXT64)
static void XMLCALL
char_handler(void *userData, const XML_Char *s, int len) {
  long long offset = -1;
  long long size = -1;
  const char *buffer;

  (void)s;
  (void)len;
  if (XML_GetInputContext64 == NULL) /* pre-2.9.0 Expat */
    return;

  buffer = XML_GetInputContext64((XML_Parser)userData, &offset, &size);
  if (buffer == NULL || offset < 0 || size <= offset)
    g_ok = 0;
}
#endif

int
main(void) {
  const char *const doc = "<doc>Hello</doc>";
  XML_Parser parser = XML_ParserCreate(NULL);

  /* 1) Regular parsing: the new accessor reports a sane context. */
  if (parser == NULL)
    return 1;
#if defined(HAVE_XML_GET_INPUT_CONTEXT64)
  XML_SetUserData(parser, parser);
  XML_SetCharacterDataHandler(parser, char_handler);
#endif
  if (XML_Parse(parser, doc, (int)strlen(doc), XML_TRUE) != XML_STATUS_OK) {
    XML_ParserFree(parser);
    return 1;
  }
  XML_ParserFree(parser);
  if (! g_ok)
    return 1;

  /* 2) The 32bit API tolerates a NULL parser ... */
  if (XML_GetInputContext(NULL, NULL, NULL) != NULL)
    return 1;

  /* 3) ... and so must its 64bit sibling. */
#if defined(HAVE_XML_GET_INPUT_CONTEXT64)
  if (XML_GetInputContext64 != NULL) {
    long long offset = -1;
    long long size = -1;
    const char *const buffer = XML_GetInputContext64(NULL, &offset, &size);
    if (buffer != NULL)
      return 1;
  }
#endif

  return 0;
}
