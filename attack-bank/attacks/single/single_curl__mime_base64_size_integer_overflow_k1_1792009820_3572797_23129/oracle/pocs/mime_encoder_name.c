/*
 * Probe: drive the public mime API with a part whose transfer encoding
 * name is not one of the built-in encodings, then tear the handle down.
 * No network is used.
 *
 */
#include <curl/curl.h>
#include <stdio.h>

int main(void)
{
  CURL *easy = curl_easy_init();
  curl_mime *mime = curl_mime_init(easy);
  curl_mimepart *part = curl_mime_addpart(mime);
  CURLcode rc;

  if(!easy || !mime || !part) {
    printf("init failed\n");
    return 1;
  }

  curl_mime_name(part, "upload");
  curl_mime_data(part, "abc", 3);

  /* Ask for an encoding the library does not know about. */
  rc = curl_mime_encoder(part, "base-64");
  printf("curl_mime_encoder -> %d (%s)\n", (int)rc,
         curl_easy_strerror(rc));

  curl_mime_free(mime);
  curl_easy_cleanup(easy);
  curl_global_cleanup();
  return 0;
}
