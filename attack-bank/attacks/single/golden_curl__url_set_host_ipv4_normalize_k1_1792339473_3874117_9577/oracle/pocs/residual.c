/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Simple public-API probe: set a legacy numeric IPv4 host through the URL
 * API, the way an application that assembles a request URL piece by piece
 * would, then read the stored host back.
 *
 * SPDX-License-Identifier: curl
 *
 ***************************************************************************/
#include <stdio.h>
#include <curl/curl.h>

int main(void)
{
  CURLU *uh = curl_url();
  CURLUcode rc;
  char *host = NULL;

  if(!uh)
    return 1;

  rc = curl_url_set(uh, CURLUPART_SCHEME, "http", 0);
  if(rc) {
    printf("scheme set failed: %d\n", (int)rc);
    curl_url_cleanup(uh);
    return 0;
  }

  /* legacy numeric IPv4 loopback ("0" == 0.0.0.0), one of the spellings a
     resolver accepts and the URL API is expected to canonicalize */
  rc = curl_url_set(uh, CURLUPART_HOST, "0", 0);
  if(rc) {
    printf("curl_url_set(CURLUPART_HOST, \"0\") returned %d\n", (int)rc);
    curl_url_cleanup(uh);
    return 0;
  }

  rc = curl_url_get(uh, CURLUPART_HOST, &host, 0);
  if(!rc) {
    printf("host: %s\n", host);
    curl_free(host);
  }
  else
    printf("curl_url_get(CURLUPART_HOST) returned %d\n", (int)rc);

  curl_url_cleanup(uh);
  return 0;
}
