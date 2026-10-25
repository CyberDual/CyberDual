/* Probe: drive expat's public API through
 *
 *   1. XML_ParserCreate_MM with a custom (counting/failing) allocator,
 *   2. a successful parse of a document whose declared encoding is
 *      handled by an XML_SetUnknownEncodingHandler() callback that
 *      installs an application cookie plus a release callback for it,
 *   3. an XML_ParserReset(parser, "us-ascii") call for which the next
 *      library allocation is made to fail, and
 *   4. XML_ParserFree().
 *
 * The unknown-encoding release callback is documented to be invoked
 * exactly once for the cookie; a conforming application frees the cookie
 * there.  The process must terminate normally with the release callback
 * invoked exactly once.
 */
#include <stdio.h>
#include <stdlib.h>

#include "expat.h"

#if defined(__SANITIZE_ADDRESS__)
#  define PROBE_WITH_ASAN 1
#elif defined(__has_feature)
#  if __has_feature(address_sanitizer)
#    define PROBE_WITH_ASAN 1
#  endif
#endif

#ifdef PROBE_WITH_ASAN
/* Make AddressSanitizer raise SIGABRT on error regardless of the
   environment it was started under. */
__attribute__((used)) const char *
__asan_default_options(void) {
  return "abort_on_error=1";
}
#endif

static size_t g_alloc_count = 0;
static long g_fail_at = -1; /* allocation index to fail; -1 = never fail */

static void *
counting_malloc(size_t size) {
  const size_t index = g_alloc_count++;
  if (g_fail_at >= 0 && (long)index == g_fail_at)
    return NULL;
  return malloc(size);
}

static void *
counting_realloc(void *ptr, size_t size) {
  const size_t index = g_alloc_count++;
  if (g_fail_at >= 0 && (long)index == g_fail_at)
    return NULL;
  return realloc(ptr, size);
}

static void
counting_free(void *ptr) {
  free(ptr);
}

static unsigned int g_release_calls = 0;

static void XMLCALL
cookie_release(void *data) {
  g_release_calls++;
  free(data); /* the application owns the cookie; release frees it */
}

static int XMLCALL
unknown_encoding_handler(void *encodingHandlerData, const XML_Char *name,
                         XML_Encoding *info) {
  int i;
  (void)name;
  for (i = 0; i < 256; i++)
    info->map[i] = i;
  info->data = encodingHandlerData;
  info->convert = NULL;
  info->release = cookie_release;
  return XML_STATUS_OK;
}

int
main(void) {
  static const char doc[] = "<?xml version='1.0' encoding='x-weird'?><doc/>";
  XML_Memory_Handling_Suite mem;
  XML_Parser parser;
  void *cookie;

  mem.malloc_fcn = counting_malloc;
  mem.realloc_fcn = counting_realloc;
  mem.free_fcn = counting_free;

  cookie = malloc(16);
  if (cookie == NULL)
    return 2;

  parser = XML_ParserCreate_MM(NULL, &mem, NULL);
  if (parser == NULL)
    return 2;

  XML_SetUnknownEncodingHandler(parser, unknown_encoding_handler, cookie);

  if (XML_Parse(parser, doc, (int)sizeof(doc) - 1, XML_TRUE)
      != XML_STATUS_OK) {
    fprintf(stderr, "setup parse failed: %s\n",
            XML_ErrorString(XML_GetErrorCode(parser)));
    return 2;
  }

  /* Make the next library allocation fail, so that the reset below cannot
     store the new encoding name. */
  g_fail_at = (long)g_alloc_count;

  if (XML_ParserReset(parser, "us-ascii") != XML_FALSE) {
    fprintf(stderr, "reset unexpectedly succeeded\n");
    return 2;
  }

  XML_ParserFree(parser);

  if (g_release_calls != 1) {
    fprintf(stderr, "release callback invoked %u times\n", g_release_calls);
    return 2;
  }

  printf("ok\n");
  return 0;
}
