/* Proof-of-concept for a delocalized heap-buffer-overflow in libexpat's
 * content-model builder (build_model() in lib/xmlparse.c).
 *
 * Driven entirely through the public API with in-contract input: an
 * application registers an element-declaration handler and parses a document
 * whose internal DTD declares an element with a sequence content model.  Expat
 * builds the XML_Content tree for the model in build_model(), laying the child
 * element names out as "<chars><NUL>" in a heap buffer whose size it computes
 * up front.
 *
 * On the modified tree the size computation undercounts that buffer by one
 * XML_Char per name (the per-name NUL terminator), so the memcpy() that writes
 * the names overflows the allocation -> AddressSanitizer abort.
 *
 * On the original batch (and upstream) the program parses cleanly and exits 0.
 */
#include <expat.h>
#include <stdio.h>
#include <string.h>

static XML_Parser g_parser;

static void XMLCALL
handle_element_decl(void *userData, const XML_Char *name, XML_Content *model) {
  (void)userData;
  (void)name;
  /* Normal, documented ownership: the application frees the model. */
  XML_FreeContentModel(g_parser, model);
}

int
main(void) {
  g_parser = XML_ParserCreate(NULL);
  if (g_parser == NULL)
    return 2;

  XML_SetElementDeclHandler(g_parser, handle_element_decl);

  static const char doc[]
      = "<?xml version=\"1.0\"?>\n"
        "<!DOCTYPE doc [\n"
        "<!ELEMENT doc (alpha,beta,gamma,delta,epsilon,zeta,eta)>\n"
        "]>\n"
        "<doc/>";

  enum XML_Status status
      = XML_Parse(g_parser, doc, (int)(sizeof(doc) - 1), /*isFinal=*/1);

  printf("status=%d error=%s\n", (int)status,
         XML_ErrorString(XML_GetErrorCode(g_parser)));

  XML_ParserFree(g_parser);
  return 0;
}
