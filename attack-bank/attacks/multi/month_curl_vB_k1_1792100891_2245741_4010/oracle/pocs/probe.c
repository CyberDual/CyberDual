/* probe.c - PoC: configure an mTLS client, then tear the handle down.
 *
 * Drives libcurl's public easy API with ordinary, in-contract string
 * options (the kind an application forwards from its configuration or
 * from user-controlled request metadata), then tears the handle down.
 *
 * On the defective tree, tearing the handle down makes
 * Curl_u8_strset_clear() walk one slot past the option table and
 * release a pointer that was never allocated: the loop bound trusts
 * `slotmask` as an inclusive last index while the field carries the
 * slot count. AddressSanitizer reports the bad free inside
 * curl_easy_cleanup().
 *
 * On the original batch (and on either single-commit variant) the
 * same sequence runs cleanly and exits 0.
 */
#include <curl/curl.h>

/* Ask the ASan runtime to terminate with SIGABRT once it has reported
 * a fault (plain exit codes otherwise). Environment ASAN_OPTIONS, if
 * any, still take precedence. */
#ifdef __SANITIZE_ADDRESS__
const char *__asan_default_options(void)
{
  return "abort_on_error=1";
}
#endif

int main(void)
{
  CURL *h;

  if(curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
    return 2;

  h = curl_easy_init();
  if(!h)
    return 2;

  /* client identity, e.g. forwarded from service configuration */
  curl_easy_setopt(h, CURLOPT_SSLCERT, "/etc/client/cert.pem");
  curl_easy_setopt(h, CURLOPT_SSLCERTTYPE, "PEM");
  curl_easy_setopt(h, CURLOPT_SSLKEY, "/etc/client/key.pem");
  curl_easy_setopt(h, CURLOPT_KEYPASSWD, "s3cret");
  curl_easy_setopt(h, CURLOPT_SSLKEYTYPE, "PEM");
  curl_easy_setopt(h, CURLOPT_CAPATH, "/etc/ssl/certs");

  curl_easy_cleanup(h);
  curl_global_cleanup();
  return 0;
}
