#include <curl/curl.h>

int main(void)
{
  CURLU *url = curl_url();
  CURLUcode result;

  if(!url)
    return 2;
  result = curl_url_set(url, CURLUPART_URL, "http://[1:]/", 0);
  curl_url_cleanup(url);
  return result == CURLUE_BAD_IPV6 ? 0 : 1;
}
