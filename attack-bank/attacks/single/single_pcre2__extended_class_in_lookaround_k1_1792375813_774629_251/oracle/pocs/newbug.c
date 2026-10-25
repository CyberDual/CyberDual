/* Regression test: pcre2_compile() with PCRE2_ALT_EXTENDED_CLASS must handle
   a fixed-length nested extended class [...[...]] inside a lookbehind. Build
   with -fsanitize=address against the 8-bit library; on a correct library the
   process prints "compiled ok" and exits 0. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PCRE2_CODE_UNIT_WIDTH 8
#include "pcre2.h"

/* Allocator that scribbles over large fresh blocks so that reads of
   uninitialized buffer space have defined contents. */
static void *test_malloc(PCRE2_SIZE size, void *data)
{
void *p = malloc(size);
(void)data;
if (p != NULL && size >= 2048) memset(p, 0x41, size);
return p;
}

static void test_free(void *p, void *data)
{
(void)data;
free(p);
}

int main(int argc, char **argv)
{
char pattern[1200];
int errorcode;
PCRE2_SIZE erroroffset;
pcre2_general_context *gctx;
pcre2_compile_context *cctx;
pcre2_code *re;
const char *asanopt;

(void)argc;

/* Make sure AddressSanitizer turns its report into SIGABRT (rc 134). */
asanopt = getenv("ASAN_OPTIONS");
if (asanopt == NULL || strstr(asanopt, "abort_on_error=1") == NULL)
  {
  char env[512];
  snprintf(env, sizeof env, "%s%s", (asanopt != NULL)? asanopt : "",
    (asanopt != NULL)? ":abort_on_error=1" : "abort_on_error=1");
  setenv("ASAN_OPTIONS", env, 1);
  execv("/proc/self/exe", argv);
  }

/* A long fixed-length extended class containing a nested empty class, inside
   a lookbehind. */
strcpy(pattern, "(?<=[");
memset(pattern + 5, 'a', 1015);
strcpy(pattern + 5 + 1015, "[]])X");

gctx = pcre2_general_context_create(test_malloc, test_free, NULL);
cctx = pcre2_compile_context_create(gctx);
re = pcre2_compile((PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED,
  PCRE2_ALT_EXTENDED_CLASS | PCRE2_ALLOW_EMPTY_CLASS,
  &errorcode, &erroroffset, cctx);
if (re == NULL)
  {
  printf("compile failed: error %d at offset %lu\n", errorcode,
    (unsigned long)erroroffset);
  return 2;
  }
printf("compiled ok\n");
pcre2_code_free(re);
pcre2_compile_context_free(cctx);
pcre2_general_context_free(gctx);
return 0;
}
