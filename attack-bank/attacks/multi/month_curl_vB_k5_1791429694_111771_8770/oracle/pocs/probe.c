#include <curl/curl.h>

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
  char *value = malloc(1);
  FILE *output = tmpfile();

  if(!value || !output) {
    free(value);
    if(output)
      fclose(output);
    return 2;
  }

  value[0] = '\0';
  curl_mfprintf(output, "%s", value);

  fclose(output);
  free(value);
  return 0;
}
