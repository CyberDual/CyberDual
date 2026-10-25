/* Residual OOB-read PoC for XML_ParseBuffer().
 *
 * XML_ParseBuffer() is documented to be called with a `len` that fits
 * inside the buffer most recently handed out by XML_GetBuffer(). This
 * reproduces an application that obtains a buffer once, feeds a first,
 * legitimately-sized chunk through XML_ParseBuffer(), and then - on a
 * later call, after parsing is already under way - passes a `len` that
 * is larger than what remains of the obtained buffer. That is a caller
 * mistake (e.g. a miscomputed byte count), but it must not translate
 * into the library reading past the end of its own buffer allocation.
 *
 * Build/run:
 *   cc -fsanitize=address,undefined -g -O0 -I <expat include dir> \
 *      residual.c -L <build dir> -lexpat -o residual
 *   LD_LIBRARY_PATH=<build dir> ./residual
 */
#include <expat.h>
#include <string.h>
#include <stdio.h>

int
main(void) {
  XML_Parser parser = XML_ParserCreate(NULL);
  if (! parser) {
    fprintf(stderr, "XML_ParserCreate failed\n");
    return 1;
  }

  void *buffer = XML_GetBuffer(parser, 16);
  if (! buffer) {
    fprintf(stderr, "XML_GetBuffer failed\n");
    return 1;
  }

  /* Fill the whole allocation handed out by XML_GetBuffer() with
   * harmless leading XML whitespace. */
  memset(buffer, ' ', 2048);

  /* First chunk: tell the parser only 3 of those bytes are meaningful
   * right now. This starts parsing and leaves ~1021 bytes of capacity
   * remaining in the already-allocated buffer. */
  if (XML_ParseBuffer(parser, 3, XML_FALSE) != XML_STATUS_OK) {
    fprintf(stderr, "unexpected parse failure on first chunk\n");
    return 1;
  }

  /* Second chunk: claim far more data than remains available in the
   * buffer obtained from XML_GetBuffer(). A correct implementation
   * rejects this with XML_ERROR_INVALID_ARGUMENT; it must not advance
   * its read window past the end of the allocation. */
  XML_ParseBuffer(parser, 5000, XML_FALSE);

  XML_ParserFree(parser);
  return 0;
}
