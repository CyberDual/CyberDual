#include <stdio.h>
#include <string.h>
#include <curl/curl.h>

struct c { const char *addr; int valid; };

static const struct c cases[] = {
  { "1:2:3:4:5:6:7:8:", 0 },
  { "::1:", 0 },
  { "1:2:3:", 0 },
  { "1:", 0 },
  { ":1", 0 },
  { ":::", 0 },
  { "1:2:3:4:5:6:7:8::", 0 },
  { "1:2:3:4:5:6:7::8:9", 0 },
  { "::", 1 },
  { "::1", 1 },
  { "1::", 1 },
  { "1::8", 1 },
  { "1:2:3:4:5:6:7:8", 1 },
  { "1:2:3:4:5:6::", 1 },
  { "1:2:3:4:5:6::8", 1 },
  { "fe80::1", 1 },
  { "::ffff:192.0.2.1", 1 },
  { "::127.0.0.1", 1 }
};

int main(void)
{
  size_t i;
  int fails = 0;
  setvbuf(stdout, NULL, _IONBF, 0);
  for(i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
    char url[256];
    CURLU *h = curl_url();
    CURLUcode rc;
    snprintf(url, sizeof(url), "http://[%s]/", cases[i].addr);
    rc = curl_url_set(h, CURLUPART_URL, url, 0);
    int got_valid = (rc == CURLUE_OK);
    printf("%-30s expect=%d got=%d %s\n", cases[i].addr, cases[i].valid,
           got_valid, (got_valid == cases[i].valid) ? "OK" : "MISMATCH");
    if(got_valid != cases[i].valid)
      fails++;
    curl_url_cleanup(h);
  }
  printf("fails=%d\n", fails);
  return fails != 0;
}
