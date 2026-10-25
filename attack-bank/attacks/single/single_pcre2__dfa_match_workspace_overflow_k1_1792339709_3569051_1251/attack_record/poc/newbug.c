#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define PCRE2_CODE_UNIT_WIDTH 8
#include "pcre2.h"

static void append(char **cursor, size_t *left, const char *text) {
  int count = snprintf(*cursor, *left, "%s", text);
  *cursor += count;
  *left -= (size_t)count;
}

static void build_pattern(int depth, char **cursor, size_t *left) {
  if (depth == 0) {
    append(cursor, left, "(?:");
    for (int tens = 0; tens < 10; tens++) {
      for (int ones = 0; ones < 10; ones++) {
        if (tens != 0 || ones != 0) append(cursor, left, "|");
        char item[3] = {(char)('a' + tens), (char)('a' + ones), 0};
        append(cursor, left, item);
      }
    }
    append(cursor, left, ")");
    return;
  }

  append(cursor, left, depth == 1 ? "(?=" : "(?=a");
  build_pattern(depth - 1, cursor, left);
  append(cursor, left, ")");
}

int main(void) {
  char pattern[8192] = {0};
  char *cursor = pattern;
  size_t left = sizeof(pattern);
  int errorcode;
  PCRE2_SIZE erroroffset;
  PCRE2_UCHAR message[256];
  pcre2_code *code;
  pcre2_match_context *context;
  pcre2_match_data *match_data;
  char subject[100];
  int workspace[1000];
  int rc;

  append(&cursor, &left, "(");
  build_pattern(23, &cursor, &left);
  append(&cursor, &left, ")");

  code = pcre2_compile((PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED, 0,
    &errorcode, &erroroffset, NULL);
  if (code == NULL) {
    pcre2_get_error_message(errorcode, message, sizeof(message));
    fprintf(stderr, "compile failed at %lu: %s\n",
      (unsigned long)erroroffset, message);
    return 2;
  }

  context = pcre2_match_context_create(NULL);
  pcre2_set_heap_limit(context, 64);
  match_data = pcre2_match_data_create(32, NULL);
  memset(subject, 'a', sizeof(subject));

  rc = pcre2_dfa_match(code, (PCRE2_SPTR)subject, sizeof(subject), 0,
    PCRE2_ANCHORED, match_data, context, workspace,
    (PCRE2_SIZE)(sizeof(workspace)/sizeof(workspace[0])));

  printf("rc=%d\n", rc);

  pcre2_match_data_free(match_data);
  pcre2_match_context_free(context);
  pcre2_code_free(code);
  return rc < 0 ? 1 : 0;
}
