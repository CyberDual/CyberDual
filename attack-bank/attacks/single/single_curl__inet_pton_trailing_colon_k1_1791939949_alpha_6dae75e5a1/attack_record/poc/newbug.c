#include <stdio.h>
#include <curl/curl.h>

/*
 * In-contract URL whose host is an IPv6 literal with one group past the
 * point where a trailing-colon check is not the failing condition.
 * curlx_inet_pton() is reached through curl_url_set().
 */
int main(void)
{
  CURLU *u;
  CURLUcode rc;
  const char *url = "http://[1:2:3:4:5:6:7:8:9:0]/";

  if(curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
    return 2;
  u = curl_url();
  if(!u)
    return 2;
  rc = curl_url_set(u, CURLUPART_URL, url, 0);
  printf("curl_url_set -> %d\n", (int)rc);
  curl_url_cleanup(u);
  curl_global_cleanup();
  return 0;
}
