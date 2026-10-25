/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_|   _ <| |___
 *                             \___|\___|_| \_\_____|
 *
 * Copyright (C) Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which you
 * should have received as part of this distribution. The terms are also
 * available at https://curl.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, subject to the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, express or implied.
 *
 * SPDX-License-Identifier: curl
 *
 * Set a CURLUPART_HOST legacy numerical IPv4 address, given in the already
 * URL encoded form that curl_url_set() expects when CURLU_URLENCODE is not
 * set.
 *
 ***************************************************************************/
#include <stdio.h>
#include <curl/curl.h>

int main(void)
{
  const char *host = "0000002130706433"; /* 127.0.0.1 with leading zeroes */
  CURLU *u = curl_url();
  CURLUcode rc;
  char *p = NULL;

  if(!u)
    return 1;

  rc = curl_url_set(u, CURLUPART_HOST, host, 0);
  if(rc) {
    printf("curl_url_set failed: %d (%s)\n", (int)rc, curl_url_strerror(rc));
    curl_url_cleanup(u);
    return 1;
  }

  rc = curl_url_get(u, CURLUPART_HOST, &p, 0);
  if(!rc) {
    printf("host: %s\n", p);
    curl_free(p);
  }

  curl_url_cleanup(u);
  return 0;
}
