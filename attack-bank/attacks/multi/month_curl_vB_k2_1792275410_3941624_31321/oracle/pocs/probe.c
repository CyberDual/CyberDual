#include <curl/curl.h>
#include <stddef.h>

int main(void)
{
  unsigned char input[1024];
  size_t index;
  char *encoded;

  for(index = 0; index < sizeof(input); index++)
    input[index] = (unsigned char)'!';

  encoded = curl_easy_escape(NULL, (const char *)input,
                             (int)sizeof(input));
  if(encoded)
    curl_free(encoded);

  return 0;
}
