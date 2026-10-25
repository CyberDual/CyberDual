#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <pcre2posix.h>

int main(void)
{
  regex_t regex;
  regmatch_t matches[1];

  if (regcomp(&regex, "(a)", 0) != 0)
    return 1;

  (void)regexec(&regex, "a", 1, matches, 0);
  regfree(&regex);
  return 0;
}
