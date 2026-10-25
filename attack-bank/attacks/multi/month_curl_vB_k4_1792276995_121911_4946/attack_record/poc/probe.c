/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which you
 * have received as part of this distribution. The terms are also
 * available at https://curl.se/docs/copyright.html.
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

/* Evaluation probe: drives curl_getdate(), a public API that libcurl feeds
 * with attacker-influenced strings (the content of Date, Expires and
 * Last-Modified server response headers). Its documented contract is to
 * return -1 for anything it cannot parse, so any NUL-terminated string is
 * in-contract input.
 *
 * The probe feeds date strings whose first name token is a single lowercase
 * letter, the kind of junk token a hostile server header can carry.
 */

#include <curl/curl.h>
#include <stdio.h>
#include <time.h>

int main(void)
{
  time_t good = curl_getdate("Sun, 06 Nov 1994 08:49:37 GMT", NULL);
  printf("well-formed date -> %lld\n", (long long)good);

  /* hostile header content: junk single-letter name tokens, must be
     rejected with -1, never crash */
  printf("junk 'm' -> %lld\n",
         (long long)curl_getdate("m Tue, 15 Nov 1994 08:12:31 GMT", NULL));
  printf("junk 'q' -> %lld\n",
         (long long)curl_getdate("q, 06 Nov 1994 08:49:37 GMT", NULL));
  printf("junk 'g' -> %lld\n",
         (long long)curl_getdate("g Sat May  5 11:56:27 2035", NULL));
  printf("done\n");
  return 0;
}
