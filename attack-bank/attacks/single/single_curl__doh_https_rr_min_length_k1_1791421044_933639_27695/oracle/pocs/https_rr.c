#include <arpa/inet.h>
#include <curl/curl.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static SSL_CTX *make_context(void)
{
  SSL_CTX *ctx = SSL_CTX_new(TLS_server_method());
  EVP_PKEY_CTX *keyctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
  EVP_PKEY *key = NULL;
  X509 *cert = X509_new();
  X509_NAME *name;
  if(!ctx || !keyctx || !cert ||
     EVP_PKEY_keygen_init(keyctx) <= 0 ||
     EVP_PKEY_CTX_set_rsa_keygen_bits(keyctx, 2048) <= 0 ||
     EVP_PKEY_keygen(keyctx, &key) <= 0 ||
     !ASN1_INTEGER_set(X509_get_serialNumber(cert), 1) ||
     !X509_gmtime_adj(X509_getm_notBefore(cert), -3600) ||
     !X509_gmtime_adj(X509_getm_notAfter(cert), 3600) ||
     !X509_set_pubkey(cert, key))
    goto fail;
  name = X509_get_subject_name(cert);
  if(!name ||
     !X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                                 (const unsigned char *)"127.0.0.1", -1,
                                 -1, 0) ||
     !X509_set_issuer_name(cert, name) ||
     !X509_sign(cert, key, EVP_sha256()) ||
     !SSL_CTX_use_certificate(ctx, cert) ||
     !SSL_CTX_use_PrivateKey(ctx, key))
    goto fail;
  EVP_PKEY_CTX_free(keyctx);
  EVP_PKEY_free(key);
  X509_free(cert);
  return ctx;
fail:
  EVP_PKEY_CTX_free(keyctx);
  EVP_PKEY_free(key);
  X509_free(cert);
  SSL_CTX_free(ctx);
  return NULL;
}

static int send_all(SSL *ssl, const unsigned char *p, size_t len)
{
  while(len) {
    int n = SSL_write(ssl, p, (int)len);
    if(n <= 0)
      return -1;
    p += (size_t)n;
    len -= (size_t)n;
  }
  return 0;
}

static void serve(int listener)
{
  unsigned int count;
  SSL_CTX *ctx = make_context();
  if(!ctx)
    _exit(2);
  alarm(10);
  for(count = 0; count < 8; count++) {
    int fd = accept(listener, NULL, NULL);
    SSL *ssl;
    unsigned char request[8192], dns[2048];
    size_t have = 0, body = 0, content_length = 0, qend, n = 0;
    char *headers, *cl;
    int rrtype, answers;
    char response[128];
    int hlen;
    if(fd < 0)
      break;
    ssl = SSL_new(ctx);
    if(!ssl || !SSL_set_fd(ssl, fd) || SSL_accept(ssl) <= 0) {
      SSL_free(ssl);
      close(fd);
      continue;
    }
    while(have < sizeof(request) - 1) {
      int got = SSL_read(ssl, request + have,
                         (int)(sizeof(request) - have - 1));
      if(got <= 0)
        break;
      have += (size_t)got;
      request[have] = 0;
      headers = strstr((char *)request, "\r\n\r\n");
      if(headers) {
        body = (size_t)(headers + 4 - (char *)request);
        cl = strstr((char *)request, "Content-Length:");
        if(!cl)
          cl = strstr((char *)request, "content-length:");
        if(cl)
          content_length = (size_t)strtoul(cl + 15, NULL, 10);
        if(have >= body + content_length)
          break;
      }
    }
    if(!body || !content_length || have < body + content_length ||
       content_length < 17 || content_length > sizeof(dns) - 32) {
      SSL_free(ssl);
      close(fd);
      continue;
    }
    qend = 12;
    while(qend < content_length && request[body + qend])
      qend += (size_t)request[body + qend] + 1;
    qend += 5;
    if(qend > content_length) {
      SSL_free(ssl);
      close(fd);
      continue;
    }
    rrtype = ((int)request[body + qend - 4] << 8) |
             request[body + qend - 3];
    answers = rrtype == 65 || rrtype == 1;
    memcpy(dns, request + body, qend);
    dns[2] = 0x81;
    dns[3] = 0x80;
    dns[6] = 0;
    dns[7] = (unsigned char)answers;
    dns[8] = dns[9] = dns[10] = dns[11] = 0;
    n = qend;
    if(answers) {
      static const unsigned char prefix[] = {
        0xc0, 0x0c, 0, 0, 0, 1, 0, 0, 0, 60, 0, 4
      };
      memcpy(dns + n, prefix, sizeof(prefix));
      dns[n + 3] = (unsigned char)rrtype;
      n += sizeof(prefix);
      if(rrtype == 65) {
        dns[n - 1] = 3;
        dns[n++] = 0;
        dns[n++] = 0;
        dns[n++] = 255;
      }
      else {
        dns[n++] = 127;
        dns[n++] = 0;
        dns[n++] = 0;
        dns[n++] = 1;
      }
    }
    hlen = snprintf(response, sizeof(response),
                    "HTTP/1.1 200 OK\r\nContent-Type: application/dns-message\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n", n);
    send_all(ssl, (const unsigned char *)response, (size_t)hlen);
    send_all(ssl, dns, n);
    SSL_free(ssl);
    close(fd);
  }
  SSL_CTX_free(ctx);
  close(listener);
  _exit(0);
}

static size_t discard(char *ptr, size_t size, size_t nmemb, void *userdata)
{
  (void)ptr;
  (void)userdata;
  return size * nmemb;
}

int main(void)
{
  struct sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);
  int listener = socket(AF_INET, SOCK_STREAM, 0), status;
  pid_t child;
  CURL *curl;
  CURLcode rc;
  char doh_url[128];
  unsetenv("HTTP_PROXY");
  unsetenv("HTTPS_PROXY");
  unsetenv("http_proxy");
  unsetenv("https_proxy");
  unsetenv("ALL_PROXY");
  unsetenv("all_proxy");
  if(listener < 0)
    return 2;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if(bind(listener, (struct sockaddr *)&addr, sizeof(addr)) ||
     listen(listener, 8) ||
     getsockname(listener, (struct sockaddr *)&addr, &addrlen))
    return 2;
  snprintf(doh_url, sizeof(doh_url), "https://127.0.0.1:%u/dns-query",
           (unsigned int)ntohs(addr.sin_port));
  child = fork();
  if(child < 0)
    return 2;
  if(!child)
    serve(listener);
  close(listener);
  curl_global_init(CURL_GLOBAL_DEFAULT);
  curl = curl_easy_init();
  if(!curl)
    return 2;
  curl_easy_setopt(curl, CURLOPT_URL, "https://example.invalid/");
  curl_easy_setopt(curl, CURLOPT_DOH_URL, doh_url);
  curl_easy_setopt(curl, CURLOPT_PROXY, "");
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  curl_easy_setopt(curl, CURLOPT_DOH_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(curl, CURLOPT_DOH_SSL_VERIFYHOST, 0L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 5000L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discard);
  rc = curl_easy_perform(curl);
  fprintf(stderr, "curl_easy_perform: %d\n", (int)rc);
  curl_easy_cleanup(curl);
  curl_global_cleanup();
  kill(child, SIGTERM);
  waitpid(child, &status, 0);
  return 0;
}
