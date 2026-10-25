#include <stdio.h>
#include <string.h>

#include "expat.h"

static int
try_parse(const char *label, const char *doc, int expect_ok) {
  XML_Parser parser = XML_ParserCreate(NULL);
  enum XML_Status status;
  enum XML_Error err;
  int ok;
  if (parser == NULL)
    return 2;
  status = XML_Parse(parser, doc, (int)strlen(doc), XML_TRUE);
  err = XML_GetErrorCode(parser);
  ok = (status == XML_STATUS_OK);
  printf("%-28s -> %s (err=%d %s) : %s\n", label,
         ok ? "OK" : "ERROR", (int)err, XML_ErrorString(err),
         (ok == expect_ok) ? "PASS" : "FAIL");
  XML_ParserFree(parser);
  return (ok == expect_ok) ? 0 : 1;
}

int
main(void) {
  int fails = 0;
  fails += try_parse("version 1.0", "<?xml version=\"1.0\"?>\n<doc/>", 1);
  fails += try_parse("version 1.1", "<?xml version=\"1.1\"?>\n<doc/>", 1);
  fails += try_parse("version 1.234",
                     "<?xml version=\"1.234\" encoding=\"UTF-8\"?>\n<doc/>",
                     1);
  fails += try_parse("version 2.0", "<?xml version=\"2.0\"?>\n<doc/>", 0);
  fails += try_parse("version 10.0", "<?xml version=\"10.0\"?>\n<doc/>", 0);
  fails += try_parse("version 1.", "<?xml version=\"1.\"?>\n<doc/>", 0);
  fails += try_parse("version 1", "<?xml version=\"1\"?>\n<doc/>", 0);
  fails += try_parse("version 1.0x", "<?xml version=\"1.0x\"?>\n<doc/>", 0);
  fails += try_parse("version empty", "<?xml version=\"\"?>\n<doc/>", 0);
  fails += try_parse("version x.0", "<?xml version=\"x.0\"?>\n<doc/>", 0);
  fails += try_parse("no decl", "<doc/>", 1);
  fails += try_parse("decl+standalone",
                     "<?xml version=\"1.0\" encoding=\"UTF-8\" "
                     "standalone=\"yes\"?>\n<doc/>",
                     1);
  printf(fails ? "SOME CHECKS FAILED\n" : "all checks passed\n");
  return fails ? 1 : 0;
}
