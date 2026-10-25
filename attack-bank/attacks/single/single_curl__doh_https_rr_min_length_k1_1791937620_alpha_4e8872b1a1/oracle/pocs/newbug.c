/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | | |_) | |
 *                             / __| | | | | _ < | |
 *                            | (__| |_| |_) | |___
 *                             \___|\___/|___/\_____|
 *
 * PoC: HTTPS-RR aware DoH resolve fed with five type-65 answers.
 * Copyright (C) Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which you
 * should have received as part of this distribution. The terms are also
 * available at https://curl.se/docs/copyright.html.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 * SPDX-License-Identifier: curl
 *
 ***************************************************************************/
/* A small HTTPS server that answers DoH queries for example.com and plays
 * a DNS server returning five valid HTTPS resource records.  libcurl is
 * asked to resolve https://example.com/ with DoH through this server.  All
 * interaction goes through the public libcurl easy API.
 */
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <openssl/ssl.h>

#include <curl/curl.h>

static const char doh_cert[] =
  "-----BEGIN CERTIFICATE-----\n"
  "MIIDGjCCAgKgAwIBAgIUFn/S7Bixk2rRkbI2fJyUuQCDYUowDQYJKoZIhvcNAQEL\n"
  "BQAwFDESMBAGA1UEAwwJMTI3LjAuMC4xMB4XDTI2MTAxNDAwMTcxNVoXDTM2MTAx\n"
  "MTAwMTcxNVowFDESMBAGA1UEAwwJMTI3LjAuMC4xMIIBIjANBgkqhkiG9w0BAQEF\n"
  "AAOCAQ8AMIIBCgKCAQEAxi3QG+VkWeBnAnFZpDTzsQThS9ENfoSNmB+SpdA38db6\n"
  "gHsMm3yVw17iDBh1yPv5SebpuFtwGqq9QBYKtD68TdwL1CsaLfs54FgfqdgOz4hz\n"
  "Mq+TtX64K6wKuBRcUS/mW7gKzxhZL820s+Zqmpdu4elH2mOQDTq2jCAFG2+BhFHY\n"
  "4Yykv1uWY1kGqlRlcgERFvW8aqBUshUfwiKlhlHBj+9TASqYWq0QarL1S7ce9RVA\n"
  "QO3u9JysuCt373/3+Wr5uaSmgQtuy3RRH5FgF8URdW75NN7ebgLE72ASZaB0+v0V\n"
  "hUQAG2PIb3WO4YS17d5P8K3cyBFT97sJpvp/wRJyUwIDAQABo2QwYjAdBgNVHQ4E\n"
  "FgQUrzbQ073a1GgRKm3HOZZdFRkgmH4wHwYDVR0jBBgwFoAUrzbQ073a1GgRKm3H\n"
  "OZZdFRkgmH4wDwYDVR0TAQH/BAUwAwEB/zAPBgNVHREECDAGhwR/AAABMA0GCSqG\n"
  "SIb3DQEBCwUAA4IBAQC9hgyvADvm8A6s4gdyaXwPLZiQn8evdsCig0pj3a/S9wBE\n"
  "9dxCdNp7/74Q2xmDHJf3rufQSBT8HQRVHUoFNRTMM+ft2aw745mwJIFUzXZs5+z1\n"
  "Po4i8i0shQzzvRZ5tfTP1hkgoi13nCY3DzCHR8KeyKBBSMkkcIxSXZJ1k4lVM3fR\n"
  "swAj7QTZ7BlxeF5b4E86WDlSfButlrBpwV0NEciaJYeIOiXEDsfgac+ARzHA8FEX\n"
  "t3/isoh5ys95QtuI0+W9f4mFy5mjnV7YvZ5tHS9QWZFl1qLfOnDV4HvQ5+k0nonB\n"
  "BuCTIn4o8Gfi6hg20RgxUagTSwNhSsHYaUxVluge\n"
  "-----END CERTIFICATE-----\n"
;

static const char doh_key[] =
  "-----BEGIN PRIVATE KEY-----\n"
  "MIIEvwIBADANBgkqhkiG9w0BAQEFAASCBKkwggSlAgEAAoIBAQDGLdAb5WRZ4GcC\n"
  "cVmkNPOxBOFL0Q1+hI2YH5Kl0Dfx1vqAewybfJXDXuIMGHXI+/lJ5um4W3Aaqr1A\n"
  "Fgq0PrxN3AvUKxot+zngWB+p2A7PiHMyr5O1frgrrAq4FFxRL+ZbuArPGFkvzbSz\n"
  "5mqal27h6UfaY5ANOraMIAUbb4GEUdjhjKS/W5ZjWQaqVGVyAREW9bxqoFSyFR/C\n"
  "IqWGUcGP71MBKpharRBqsvVLtx71FUBA7e70nKy4K3fvf/f5avm5pKaBC27LdFEf\n"
  "kWAXxRF1bvk03t5uAsTvYBJloHT6/RWFRAAbY8hvdY7hhLXt3k/wrdzIEVP3uwmm\n"
  "+n/BEnJTAgMBAAECggEAJHFdQRv8SajB2ImmjdMs8uLuWSZOwHK4pP49Vd+FwiOh\n"
  "ugmmplsRITWsBMRkuHpZe7UKSpwJ7+Vw2r2S51VYBEtQI4pvjIATwwUBgZaZFqHF\n"
  "aU8vLuNoE6PV7sbTuRxBVKFx32ZfVDKUJbbb6KSrP8DJtUe0wDIJSphCyCpHm2Cs\n"
  "f/1rmT+7dxMevtdU+XWeykCMYZufpEd/n77Tmk+9BdEfwhx8S0TyXtLsiSkVC5Ch\n"
  "C8ZTXmPW4qj7N5BkOpSeHJWurIYK+fA879dSWMiyoZFUEsmnqSEkxuxWqQtlG+0z\n"
  "GqpQCniI6o6wXZTLHxnaPkKyNP6WKYEcPIvUdCemrQKBgQD2bR9KXUoT7uT3Z9Xw\n"
  "jxKirfFdG8tSl03RVrLVKE7Hs2ORATBRbloXSFDZdLHxJ6fqhOTRz2KTlDeOr3Q6\n"
  "aeYPgmdEHkkVyB+/+JCE9lToCKvpOhccT5xpz+5guc1z49cI6eSKHOWGC4uWuQa/\n"
  "isxiBt+0x7TY94kse6kedcMtTQKBgQDN4NaYcvhatEt9qq9nB8YedRjAVr6iicQR\n"
  "prmHCU0dXdrBfuexMTqFPe2WetOjR3Wri1c1J7U2Q3J2xnFYnlFQR7TzdG2UsXrO\n"
  "OM81g4iU3LLMbVLn/MyC81JZdHdJtefyde9yUuW4wqQjKvaf0Rgqz+xJ+WXwxqpK\n"
  "ktyG4ezOHwKBgQCwEDcTAqQyqbSdr2qDfTVrVavlQj56u++1aaRL94iR1fGFYLwM\n"
  "8jMb/2cEdwPLsPGf9WMnJGzh1Dv9jg4UziS5YAo3+QturJVCq21WEkmHnup0sDRi\n"
  "3d57GzzNjvLjoh25GKNYqlyy8PzCq1/O6nQsdYzPtP/ZJMK1LYUfZjQU4QKBgQDC\n"
  "APaXr/eCmdVtdTNWlofGWbfaP6PizImbzI2Vx1kIrFGg9vJkEizANEw3+kSG36bt\n"
  "4R9rsqBboxExBez3gjDDwt6sb/F3JgvAWt4/4lQJWzON2hM/EwSw/ZODQnHKXwVU\n"
  "gHgENkPNaHV9wlWm4kWwykAysTazvrHoFh6DaFBbJwKBgQCYT4nTGKyDP4AOf0Vr\n"
  "LtqFhObUCiAEcKCv5i+VBV4kztQ8LrNa+sX6Q/G8NRshbQn9l9vOVtSPEqx2S+Bx\n"
  "DDyHPqtJkgBQapM7LxsrVTzYfKpAAbLyldlj7c5YHucoLHw506g8mhnQpTO90+HA\n"
  "01D8S7QKrqtS/PhqPcyc0MYaPQ==\n"
  "-----END PRIVATE KEY-----\n"
;

static uint16_t g_port;

static int read_n(SSL *ssl, void *buf, size_t n)
{
  unsigned char *p = buf;
  size_t got = 0;
  while(got < n) {
    int r = SSL_read(ssl, p + got, (int)(n - got));
    if(r <= 0)
      return -1;
    got += (size_t)r;
  }
  return 0;
}

static int write_n(SSL *ssl, const void *buf, size_t n)
{
  const unsigned char *p = buf;
  size_t sent = 0;
  while(sent < n) {
    int r = SSL_write(ssl, p + sent, (int)(n - sent));
    if(r <= 0)
      return -1;
    sent += (size_t)r;
  }
  return 0;
}

/* Build a DNS response for the query in `req`.  The question section is
 * echoed.  For HTTPS queries the answer section carries five records,
 * otherwise a plain A answer is returned.
 */
static size_t build_response(const unsigned char *req, size_t reqlen,
                             unsigned char *out, size_t outsize)
{
  size_t qend = 12; /* header size */
  size_t o;
  unsigned qtype = 0;

  /* find the end of the question name */
  if(reqlen < 17)
    return 0;
  while(qend < reqlen && req[qend]) {
    if((size_t)req[qend] + 1 > reqlen - qend)
      return 0;
    qend += (size_t)req[qend] + 1;
  }
  qend++; /* root label */
  if((qend + 4) > reqlen)
    return 0;
  qtype = ((unsigned)req[qend] << 8) | req[qend + 1];
  qend += 4; /* qtype + qclass */

  o = 0;
  if((qend + 128) > outsize)
    return 0;
  memcpy(out, req, qend);
  o = qend;
  out[2] = (unsigned char)0x81; /* QR | RD */
  out[3] = (unsigned char)0x80; /* RA */

  if(qtype == 65) { /* HTTPS RR: five answers */
    int i;
    out[7] = 5; /* ANCOUNT */
    for(i = 0; i < 5; i++) {
      out[o++] = 0; /* name: root */
      out[o++] = 0; out[o++] = 65; /* TYPE65 */
      out[o++] = 0; out[o++] = 1; /* CLASS IN */
      out[o++] = 0; out[o++] = 0; out[o++] = 0; out[o++] = 0; /* TTL 0 */
      out[o++] = 0; out[o++] = 3; /* RDLENGTH */
      out[o++] = 0; out[o++] = 1; /* SvcPriority 1 */
      out[o++] = 0; /* TargetName: root */
    }
  }
  else if(qtype == 1) { /* A: one answer */
    out[7] = 1;
    out[o++] = 0; /* name: root */
    out[o++] = 0; out[o++] = 1; /* TYPE A */
    out[o++] = 0; out[o++] = 1; /* CLASS IN */
    out[o++] = 0; out[o++] = 0; out[o++] = 0; out[o++] = 0; /* TTL 0 */
    out[o++] = 0; out[o++] = 4; /* RDLENGTH */
    out[o++] = 127; out[o++] = 0; out[o++] = 0; out[o++] = 1;
  }
  else { /* AAAA and friends: NOERROR, no answers */
    out[7] = 0;
  }
  out[6] = 0;
  return o;
}

static void *server_thread(void *unused)
{
  int lfd;
  struct sockaddr_in addr;
  SSL_CTX *ctx = NULL;
  BIO *bio = NULL;
  X509 *cert = NULL;
  EVP_PKEY *key = NULL;
  (void)unused;

  signal(SIGPIPE, SIG_IGN);
  lfd = socket(AF_INET, SOCK_STREAM, 0);
  if(lfd < 0)
    return NULL;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;
  if(bind(lfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    goto out;
  {
    socklen_t alen = sizeof(addr);
    if(getsockname(lfd, (struct sockaddr *)&addr, &alen) < 0)
      goto out;
    g_port = ntohs(addr.sin_port);
  }
  if(listen(lfd, 16) < 0)
    goto out;

  ctx = SSL_CTX_new(TLS_server_method());
  if(!ctx)
    goto out;
  bio = BIO_new_mem_buf(doh_cert, -1);
  cert = bio ? PEM_read_bio_X509(bio, NULL, NULL, NULL) : NULL;
  BIO_free(bio);
  bio = BIO_new_mem_buf(doh_key, -1);
  key = bio ? PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL) : NULL;
  BIO_free(bio);
  if(!cert || !key || !SSL_CTX_use_certificate(ctx, cert) ||
     !SSL_CTX_use_PrivateKey(ctx, key) || SSL_CTX_check_private_key(ctx) != 1)
    goto out;

  for(;;) {
    int cfd = accept(lfd, NULL, NULL);
    SSL *ssl;
    char hdrs[8192];
    size_t hlen = 0;
    long clen = -1;
    unsigned char req[512];
    unsigned char resp[512];
    char http[8192];
    size_t resplen, httplen;

    if(cfd < 0)
      continue;
    ssl = SSL_new(ctx);
    if(!ssl) {
      close(cfd);
      continue;
    }
    SSL_set_fd(ssl, cfd);
    if(SSL_accept(ssl) <= 0) {
      SSL_free(ssl);
      close(cfd);
      continue;
    }

    /* read head and body; TLS may deliver them in one record */
    hdrs[0] = 0;
    for(;;) {
      char *end, *cl;
      size_t hdr_end, have;
      if(hlen >= sizeof(hdrs) - 1)
        break;
      {
        int r = SSL_read(ssl, hdrs + hlen, (int)(sizeof(hdrs) - 1 - hlen));
        if(r <= 0)
          break;
        hlen += (size_t)r;
        hdrs[hlen] = 0;
      }
      end = strstr(hdrs, "\r\n\r\n");
      if(!end)
        continue;
      hdr_end = (size_t)(end - hdrs) + 4;
      cl = strstr(hdrs, "Content-Length:");
      if(!cl)
        cl = strstr(hdrs, "content-length:");
      if(!cl)
        break;
      clen = strtol(cl + 15, NULL, 10);
      if(clen <= 0 || (size_t)clen > (long)sizeof(req))
        break;
      have = hlen - hdr_end;
      if(have > (size_t)clen)
        have = (size_t)clen;
      memcpy(req, hdrs + hdr_end, have);
      if(have < (size_t)clen &&
         read_n(ssl, req + have, (size_t)clen - have) < 0)
        break;
      hlen = (size_t)-1; /* mark: request complete */
      break;
    }
    if(hlen == (size_t)-1)
      resplen = build_response(req, (size_t)clen, resp, sizeof(resp));
    else
      resplen = 0;
    if(!resplen) {
      SSL_free(ssl);
      close(cfd);
      continue;
    }
    httplen = (size_t)snprintf(http, sizeof(http),
                               "HTTP/1.1 200 OK\r\n"
                               "Content-Type: application/dns-message\r\n"
                               "Content-Length: %lu\r\n"
                               "Connection: close\r\n\r\n",
                               (unsigned long)resplen);
    if(write_n(ssl, http, httplen) < 0 || write_n(ssl, resp, resplen) < 0) {
      SSL_free(ssl);
      close(cfd);
      continue;
    }
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(cfd);
  }
out:
  SSL_CTX_free(ctx);
  X509_free(cert);
  EVP_PKEY_free(key);
  close(lfd);
  return NULL;
}

int main(void)
{
  pthread_t srv;
  CURL *easy;
  struct curl_blob cablob;
  char dohurl[128];
  CURLcode rc;
  int ret = 0;

  if(pthread_create(&srv, NULL, server_thread, NULL) != 0)
    return 2;
  /* wait for the listening socket */
  while(!g_port)
    usleep(1000);

  if(curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
    ret = 2;
    goto done;
  }
  easy = curl_easy_init();
  if(!easy) {
    curl_global_cleanup();
    ret = 2;
    goto done;
  }
  cablob.data = (void *)doh_cert;
  cablob.len = strlen(doh_cert);
  cablob.flags = CURL_BLOB_COPY;
  snprintf(dohurl, sizeof(dohurl), "https://127.0.0.1:%u/dns-query",
           (unsigned)g_port);
  curl_easy_setopt(easy, CURLOPT_URL, "https://example.com/");
  curl_easy_setopt(easy, CURLOPT_DOH_URL, dohurl);
  curl_easy_setopt(easy, CURLOPT_CAINFO_BLOB, &cablob);
  curl_easy_setopt(easy, CURLOPT_VERBOSE, 1L);
  curl_easy_setopt(easy, CURLOPT_TIMEOUT, 20L);
  rc = curl_easy_perform(easy);
  fprintf(stderr, "curl_easy_perform returned %d (%s)\n", (int)rc,
          rc ? curl_easy_strerror(rc) : "ok");
  curl_easy_cleanup(easy);
  curl_global_cleanup();

done:
  (void)srv;
  return ret;
}
