#include <arpa/inet.h>
#include <curl/curl.h>
#include <openssl/ssl.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

static void serve(int listener)
{
  EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
  EVP_PKEY *key = NULL;
  X509 *cert = X509_new();
  X509_NAME *name;
  SSL_CTX *ctx = SSL_CTX_new(TLS_server_method());
  if(!pctx || !cert || !ctx ||
     EVP_PKEY_keygen_init(pctx) <= 0 ||
     EVP_PKEY_CTX_set_rsa_keygen_bits(pctx, 2048) <= 0 ||
     EVP_PKEY_keygen(pctx, &key) <= 0)
    _exit(7);
  X509_set_version(cert, 2);
  ASN1_INTEGER_set(X509_get_serialNumber(cert), 1);
  X509_gmtime_adj(X509_get_notBefore(cert), -60);
  X509_gmtime_adj(X509_get_notAfter(cert), 3600);
  X509_set_pubkey(cert, key);
  name = X509_get_subject_name(cert);
  X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                             (unsigned char *)"127.0.0.1", -1, -1, 0);
  X509_set_issuer_name(cert, name);
  if(!X509_sign(cert, key, EVP_sha256()) ||
     SSL_CTX_use_certificate(ctx, cert) <= 0 ||
     SSL_CTX_use_PrivateKey(ctx, key) <= 0)
    _exit(8);
  X509_free(cert);
  EVP_PKEY_free(key);
  EVP_PKEY_CTX_free(pctx);
  for(;;) {
    unsigned char request[4096], dns[4096], response[8192];
    size_t used = 0, header_len, body_len, qlen, dlen = 0, rlen;
    char *headers, *length;
    unsigned short qtype;
    int fd = accept(listener, NULL, NULL);
    SSL *ssl;
    if(fd < 0)
      _exit(1);
    ssl = SSL_new(ctx);
    if(!ssl || !SSL_set_fd(ssl, fd) || SSL_accept(ssl) <= 0)
      _exit(9);
    for(;;) {
      int n = SSL_read(ssl, request + used, (int)(sizeof(request) - used));
      if(n <= 0)
        _exit(2);
      used += (size_t)n;
      if(used == sizeof(request))
        _exit(3);
      request[used] = 0;
      headers = strstr((char *)request, "\r\n\r\n");
      if(headers)
        break;
    }
    header_len = (size_t)(headers + 4 - (char *)request);
    length = strstr((char *)request, "Content-Length:");
    if(!length)
      _exit(4);
    body_len = (size_t)strtoul(length + 15, NULL, 10);
    if(body_len < 16 || header_len + body_len > sizeof(request))
      _exit(5);
    while(used < header_len + body_len) {
      int n = SSL_read(ssl, request + used, (int)(sizeof(request) - used));
      if(n <= 0)
        _exit(6);
      used += (size_t)n;
    }
    qlen = body_len - 12;
    qtype = (unsigned short)((request[header_len + body_len - 4] << 8) |
                              request[header_len + body_len - 3]);
    memset(dns, 0, 12);
    dns[2] = 0x81;
    dns[3] = 0x80;
    dns[5] = 1;
    dns[7] = 1;
    dlen = 12;
    memcpy(dns + dlen, request + header_len + 12, qlen);
    dlen += qlen;
    dns[dlen++] = 0xc0;
    dns[dlen++] = 0x0c;
    dns[dlen++] = (unsigned char)(qtype >> 8);
    dns[dlen++] = (unsigned char)qtype;
    dns[dlen++] = 0;
    dns[dlen++] = 1;
    dns[dlen++] = 0;
    dns[dlen++] = 0;
    dns[dlen++] = 0;
    dns[dlen++] = 60;
    if(qtype == 65) {
      static const unsigned char rdata[] = {0, 1, 0, 0, 2, 0};
      dns[dlen++] = 0;
      dns[dlen++] = sizeof(rdata);
      memcpy(dns + dlen, rdata, sizeof(rdata));
      dlen += sizeof(rdata);
    }
    else if(qtype == 28) {
      dns[dlen++] = 0;
      dns[dlen++] = 16;
      memset(dns + dlen, 0, 16);
      dns[dlen + 15] = 1;
      dlen += 16;
    }
    else {
      dns[dlen++] = 0;
      dns[dlen++] = 4;
      dns[dlen++] = 127;
      dns[dlen++] = 0;
      dns[dlen++] = 0;
      dns[dlen++] = 1;
    }
    rlen = (size_t)snprintf((char *)response, sizeof(response),
                           "HTTP/1.1 200 OK\r\n"
                           "Content-Type: application/dns-message\r\n"
                           "Content-Length: %zu\r\n"
                           "Connection: close\r\n\r\n", dlen);
    memcpy(response + rlen, dns, dlen);
    SSL_write(ssl, response, (int)(rlen + dlen));
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(fd);
  }
}

int main(void)
{
  struct sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);
  CURL *easy;
  char doh_url[128];
  pid_t server;
  int listener = socket(AF_INET, SOCK_STREAM, 0);
  if(listener < 0)
    return 1;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;
  if(bind(listener, (struct sockaddr *)&addr, sizeof(addr)) ||
     listen(listener, 8) ||
     getsockname(listener, (struct sockaddr *)&addr, &addrlen))
    return 2;
  server = fork();
  if(server < 0)
    return 3;
  if(!server) {
    prctl(PR_SET_PDEATHSIG, SIGTERM);
    serve(listener);
  }
  close(listener);
  snprintf(doh_url, sizeof(doh_url), "https://127.0.0.1:%u/dns-query",
           ntohs(addr.sin_port));
  unsetenv("http_proxy");
  unsetenv("HTTP_PROXY");
  unsetenv("https_proxy");
  unsetenv("HTTPS_PROXY");
  unsetenv("all_proxy");
  unsetenv("ALL_PROXY");
  if(curl_global_init(CURL_GLOBAL_DEFAULT))
    return 4;
  easy = curl_easy_init();
  if(!easy)
    return 5;
  curl_easy_setopt(easy, CURLOPT_URL, "https://foo.example.com/");
  curl_easy_setopt(easy, CURLOPT_DOH_URL, doh_url);
  curl_easy_setopt(easy, CURLOPT_NOPROXY, "*");
  curl_easy_setopt(easy, CURLOPT_TIMEOUT_MS, 5000L);
  curl_easy_setopt(easy, CURLOPT_DOH_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(easy, CURLOPT_DOH_SSL_VERIFYHOST, 0L);
  curl_easy_setopt(easy, CURLOPT_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(easy, CURLOPT_SSL_VERIFYHOST, 0L);
  curl_easy_perform(easy);
  curl_easy_cleanup(easy);
  curl_global_cleanup();
  kill(server, SIGTERM);
  waitpid(server, NULL, 0);
  return 0;
}
