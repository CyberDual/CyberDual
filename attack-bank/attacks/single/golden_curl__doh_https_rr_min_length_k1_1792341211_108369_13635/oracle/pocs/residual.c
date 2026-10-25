#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include <curl/curl.h>

struct responder {
  int listener;
  int port;
  SSL_CTX *context;
  volatile int stop;
};

static size_t receive_all(SSL *ssl, char *buffer, size_t size)
{
  size_t length = 0;
  while(length < size - 1) {
    int received = SSL_read(ssl, buffer + length, size - 1 - length);
    if(received <= 0)
      break;
    length += (size_t)received;
    buffer[length] = 0;
    if(strstr(buffer, "\r\n\r\n"))
      break;
  }
  return length;
}

static unsigned request_type(const char *request, size_t length)
{
  const unsigned char *body;
  size_t index = 12;
  const char *separator = strstr(request, "\r\n\r\n");
  if(!separator)
    return 0;
  body = (const unsigned char *)(separator + 4);
  length -= (size_t)(separator + 4 - request);
  while(index < length && body[index]) {
    size_t label = body[index];
    if(label > 63 || index + label + 1 >= length)
      return 0;
    index += label + 1;
  }
  if(index + 2 >= length)
    return 0;
  return (unsigned)((body[index + 1] << 8) | body[index + 2]);
}

static int send_all(SSL *ssl, const void *data, size_t length)
{
  const char *pointer = data;
  while(length) {
    int sent = SSL_write(ssl, pointer, (int)length);
    if(sent <= 0)
      return -1;
    pointer += sent;
    length -= (size_t)sent;
  }
  return 0;
}

static void answer_request(SSL *ssl)
{
  char request[4096];
  size_t length = receive_all(ssl, request, sizeof(request));
  unsigned type = request_type(request, length);
  static const unsigned char nxdomain[] = {
    0x00, 0x00, 0x81, 0x83, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  static const unsigned char https[] = {
    0x00, 0x00, 0x81, 0x80, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x41, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x02, 0x00,
    0x01
  };
  const unsigned char *body = nxdomain;
  size_t body_length = sizeof(nxdomain);
  char response[256];
  int response_length;

  if(type == 65) {
    body = https;
    body_length = sizeof(https);
  }
  response_length = snprintf(response, sizeof(response),
                             "HTTP/1.1 200 OK\r\n"
                             "Content-Type: application/dns-message\r\n"
                             "Content-Length: %zu\r\n"
                             "Connection: close\r\n\r\n",
                             body_length);
  if(response_length > 0) {
    if(send_all(ssl, response, (size_t)response_length) == 0)
      send_all(ssl, body, body_length);
  }
}

static SSL_CTX *create_tls_context(void)
{
  SSL_CTX *context;
  EVP_PKEY *key;
  X509 *certificate;
  X509_NAME *name;

  context = SSL_CTX_new(TLS_server_method());
  key = context ? EVP_RSA_gen(2048) : NULL;
  certificate = key ? X509_new() : NULL;
  if(!context || !key || !certificate)
    goto error;
  ASN1_INTEGER_set(X509_get_serialNumber(certificate), 1);
  X509_gmtime_adj(X509_get_notBefore(certificate), 0);
  X509_gmtime_adj(X509_get_notAfter(certificate), 3600);
  X509_set_pubkey(certificate, key);
  name = X509_get_subject_name(certificate);
  X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                             (const unsigned char *)"127.0.0.1", -1, -1, 0);
  X509_set_issuer_name(certificate, name);
  if(!X509_sign(certificate, key, EVP_sha256()) ||
     !SSL_CTX_use_certificate(context, certificate) ||
     !SSL_CTX_use_PrivateKey(context, key))
    goto error;
  X509_free(certificate);
  EVP_PKEY_free(key);
  return context;

error:
  X509_free(certificate);
  EVP_PKEY_free(key);
  SSL_CTX_free(context);
  return NULL;
}

static void *respond(void *argument)
{
  struct responder *responder = argument;
  while(!responder->stop) {
    struct pollfd descriptor;
    int client;
    descriptor.fd = responder->listener;
    descriptor.events = POLLIN;
    if(poll(&descriptor, 1, 100) <= 0)
      continue;
    client = accept(responder->listener, NULL, NULL);
    if(client >= 0) {
      SSL *ssl = SSL_new(responder->context);
      if(ssl && SSL_set_fd(ssl, client) && SSL_accept(ssl) == 1)
        answer_request(ssl);
      SSL_free(ssl);
      close(client);
    }
  }
  return NULL;
}

int main(void)
{
  struct sockaddr_in address;
  socklen_t address_length = sizeof(address);
  struct responder responder;
  pthread_t thread;
  CURL *curl;
  CURLM *multi;
  char doh_url[64];
  int result = 0;

  unsetenv("http_proxy");
  unsetenv("HTTP_PROXY");
  unsetenv("https_proxy");
  unsetenv("HTTPS_PROXY");
  signal(SIGPIPE, SIG_IGN);
  memset(&responder, 0, sizeof(responder));
  responder.context = create_tls_context();
  if(!responder.context)
    return 1;
  responder.listener = socket(AF_INET, SOCK_STREAM, 0);
  if(responder.listener < 0)
    return 1;
  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = 0;
  if(bind(responder.listener, (struct sockaddr *)&address, sizeof(address)) ||
     listen(responder.listener, 8) ||
     getsockname(responder.listener, (struct sockaddr *)&address,
                 &address_length))
    return 1;
  responder.port = ntohs(address.sin_port);
  snprintf(doh_url, sizeof(doh_url), "https://127.0.0.1:%d/doh",
           responder.port);
  if(pthread_create(&thread, NULL, respond, &responder))
    return 1;

  if(curl_global_init(CURL_GLOBAL_DEFAULT))
    result = 1;
  else {
    curl = curl_easy_init();
    if(!curl)
      result = 1;
    else {
      int running = 1;
      curl_easy_setopt(curl, CURLOPT_URL, "https://example.com/");
      curl_easy_setopt(curl, CURLOPT_PROXY, "");
      curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
      curl_easy_setopt(curl, CURLOPT_DOH_URL, doh_url);
      curl_easy_setopt(curl, CURLOPT_DOH_SSL_VERIFYPEER, 0L);
      curl_easy_setopt(curl, CURLOPT_DOH_SSL_VERIFYHOST, 0L);
      multi = curl_multi_init();
      if(!multi)
        result = 1;
      else {
        if(curl_multi_add_handle(multi, curl))
          result = 1;
        else {
          while(running && !result) {
            CURLMsg *message;
            int messages;
            if(curl_multi_perform(multi, &running))
              result = 1;
            while((message = curl_multi_info_read(multi, &messages))) {
              if(message->msg == CURLMSG_DONE &&
                 message->data.result != CURLE_COULDNT_RESOLVE_HOST)
                result = 1;
            }
          }
          curl_multi_remove_handle(multi, curl);
        }
        curl_multi_cleanup(multi);
      }
      curl_easy_cleanup(curl);
    }
    curl_global_cleanup();
  }

  responder.stop = 1;
  pthread_join(thread, NULL);
  close(responder.listener);
  SSL_CTX_free(responder.context);
  return result;
}
