/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |_) | |
 *                             / __| | | |  _ \| |
 *                            | (__| |_| | |_) | |___
 *                             \___|\___|_| \_\_____|
 *
 * Copyright (C) Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * SPDX-License-Identifier: curl
 *
 * A probe for the curl URL API: feed it a URL whose host is an IPv6
 * literal with too many hex groups and check that it is rejected
 * without upsetting memory debugging.
 *
 ***************************************************************************/
#include <stdio.h>
#include <curl/curl.h>

int main(void)
{
  const char *url = "http://[1:2:3:4:5:6:7:8:9]/";
  CURLU *uh = curl_url();
  CURLUcode rc;

  if(!uh)
    return 2;

  /* a well-formed address still parses, to show the API is working */
  rc = curl_url_set(uh, CURLUPART_URL, "http://[1:2:3:4:5:6:7:8]/", 0);
  printf("valid address:   curl_url_set() -> %d (CURLUE_OK=%d)\n",
         (int)rc, (int)CURLUE_OK);
  curl_url_cleanup(uh);

  uh = curl_url();
  if(!uh)
    return 2;
  rc = curl_url_set(uh, CURLUPART_URL, url, 0);
  printf("overlong address: curl_url_set() -> %d\n", (int)rc);
  curl_url_cleanup(uh);
  return 0;
}
