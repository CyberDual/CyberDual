#include <curl/curl.h>

int main(void)
{
  CURLU *url = curl_url();
  CURLUcode result;

  if(!url)
    return 2;

  result = curl_url_set(url, CURLUPART_URL,
                        "http://[1:2:3:4:5:6:7:8:9:10]/", 0);
  curl_url_cleanup(url);
  return result == CURLUE_BAD_IPV6 ? 0 : 1;
}
