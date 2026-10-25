#include <curl/curl.h>
#include <stdio.h>

/* Trigger: set a host name carrying a zone id, then replace the host with a
   legacy numerical IPv4 address (which the new code normalizes), then clean
   up. The stale zone id pointer is freed twice. */

int main(void)
{
  CURLU *u = curl_url();
  CURLUcode uc;
  char *host = NULL;

  if(!u)
    return 1;

  /* a host name with a zone id */
  uc = curl_url_set(u, CURLUPART_HOST, "[fe80::1%25eth0]", 0);
  if(uc) {
    fprintf(stderr, "set zoneid host failed: %d\n", (int)uc);
    return 1;
  }

  /* replace it with a legacy numerical IPv4 address */
  uc = curl_url_set(u, CURLUPART_HOST, "0177.1", 0);
  if(uc) {
    fprintf(stderr, "set numerical host failed: %d\n", (int)uc);
    return 1;
  }

  uc = curl_url_get(u, CURLUPART_HOST, &host, 0);
  if(uc) {
    fprintf(stderr, "get host failed: %d\n", (int)uc);
    return 1;
  }
  printf("host: %s\n", host);
  fflush(stdout);
  curl_free(host);

  curl_url_cleanup(u);
  printf("done\n");
  return 0;
}
