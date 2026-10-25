/* Probe: drive libcurl's public mprintf API with an attacker-controlled
   format string. A correctly behaving library prints the argument string
   and exits 0; a library with a memory-safety fault in the printf engine
   aborts under AddressSanitizer while formatting.

   Build against an ASan-instrumented static libcurl, e.g.:
     gcc -fsanitize=address -no-pie -g -O1 -I<curl>/include probe.c \
         <build>/lib/libcurl.a -lpthread */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#ifdef __SANITIZE_ADDRESS__
/* Terminate with SIGABRT when AddressSanitizer reports an error, instead
   of the default plain exit. */
const char *__asan_default_options(void)
{
  return "abort_on_error=1";
}
#endif

int main(void)
{
  /* Exactly sized heap string: two bytes, 'A' + NUL, so any read past the
     terminating NUL lands outside the allocation. */
  char *arg = (char *)malloc(2);
  char *out;
  if(!arg)
    return 2;
  arg[0] = 'A';
  arg[1] = '\0';

  /* Explicit negative precision on a string conversion: in-contract input
     for curl's printf dialect (the '-' is parsed by the engine itself). */
  out = curl_maprintf("%.-5s", arg);
  if(out) {
    printf("formatted: \"%s\"\n", out);
    curl_free(out);
  }
  else {
    printf("formatting failed\n");
  }

  free(arg);
  return 0;
}
