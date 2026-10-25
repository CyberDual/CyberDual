// Rejected-input probe: a lossless WebP whose meta-Huffman image forces a
// group remap (num_htree_groups < num_htree_groups_max) and whose
// Huffman-table section is shorter than the advertised group count needs.
// Reached through the public decode API only.
#include <stdio.h>
#include <stdlib.h>
#include "webp/decode.h"

static const unsigned char kTruncatedWebP[] = {
    0x52, 0x49, 0x46, 0x46, 0x2e, 0x00, 0x00, 0x00, 0x57, 0x45, 0x42, 0x50,
    0x56, 0x50, 0x38, 0x4c, 0x22, 0x00, 0x00, 0x00, 0x2f, 0x27, 0x00, 0x00,
    0x00, 0x04, 0x03, 0x04, 0x90, 0x46, 0xa3, 0xaa, 0xff, 0x88, 0x88, 0xbf,
    0x9d, 0xc6, 0x22, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

int main(int argc, char** argv) {
  const unsigned char* data = kTruncatedWebP;
  size_t size = sizeof(kTruncatedWebP);
  unsigned char* file_data = NULL;

  if (argc > 1) {
    FILE* f = fopen(argv[1], "rb");
    long n;
    if (f == NULL) return 2;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    file_data = (unsigned char*)malloc(n);
    if (file_data == NULL || fread(file_data, 1, n, f) != (size_t)n) return 2;
    fclose(f);
    data = file_data;
    size = (size_t)n;
  }

  {
    int width = 0, height = 0;
    unsigned char* rgba = WebPDecodeRGBA(data, size, &width, &height);
    if (rgba == NULL) {
      printf("decode rejected (%dx%d)\n", width, height);
    } else {
      printf("decoded %dx%d\n", width, height);
      free(rgba);
    }
  }
  free(file_data);
  return 0;
}
