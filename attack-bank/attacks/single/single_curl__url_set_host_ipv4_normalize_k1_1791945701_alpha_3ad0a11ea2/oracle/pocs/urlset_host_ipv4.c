/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___|_|_| \_\_____|
 *
 * Copyright (C) Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at https://curl.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 * SPDX-License-Identifier: curl
 *
 ***************************************************************************/
#include <stdio.h>
#include <curl/mprintf.h>
#include <curl/curl.h>

/* the URL encoded legacy octal IPv4 number
   "00000000000000000000000000000000000000001", which normalizes to the
   dotted-decimal address 0.0.0.1 */
static const char hostpart[] =
  "%30%30%30%30%30%30%30%30%30%30"
  "%30%30%30%30%30%30%30%30%30%30"
  "%30%30%30%30%30%30%30%30%30%30"
  "%30%30%30%30%30%30%30%30%30%30"
  "%31";

int main(void)
{
  CURLU *u = curl_url();
  CURLUcode rc;
  char *host = NULL;

  if(!u)
    return 1;

  rc = curl_url_set(u, CURLUPART_HOST, hostpart, 0);
  if(rc != CURLUE_OK) {
    curl_mfprintf(stderr, "curl_url_set(CURLUPART_HOST) returned %d\n",
                  (int)rc);
    curl_url_cleanup(u);
    return 1;
  }

  rc = curl_url_get(u, CURLUPART_HOST, &host, 0);
  if(rc == CURLUE_OK) {
    curl_mprintf("host: %s\n", host);
    curl_free(host);
  }

  curl_url_cleanup(u);
  return 0;
}
