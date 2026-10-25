// Probe: import a packed RGB565 image, encode it to WebP and decode it back.
// Usage: residual <rgb565_file> [width] [height]
// The input file must hold width*height packed 16-bit RGB565 samples.
#include <stdio.h>
#include <stdlib.h>

#include "webp/decode.h"
#include "webp/encode.h"
#include "../extras/extras.h"

int main(int argc, char* argv[]) {
  const char* path = (argc > 1) ? argv[1] : "input.565";
  const int width = (argc > 2) ? atoi(argv[2]) : 5;
  const int height = (argc > 3) ? atoi(argv[3]) : 3;
  const size_t num_bytes = (size_t)2 * width * height;
  uint8_t* rgb565 = (uint8_t*)malloc(num_bytes);
  FILE* f = fopen(path, "rb");
  WebPPicture pic;
  WebPConfig config;
  WebPMemoryWriter writer;
  uint8_t* decoded;
  int ok = 0;

  if (rgb565 == NULL) {
    fprintf(stderr, "out of memory\n");
    return 2;
  }
  if (f != NULL) {
    if (fread(rgb565, 1, num_bytes, f) != num_bytes) {
      fprintf(stderr, "short input\n");
      fclose(f);
      free(rgb565);
      return 2;
    }
    fclose(f);
  } else {
    size_t i;
    for (i = 0; i < num_bytes; ++i) rgb565[i] = (uint8_t)(i * 37 + 11);
  }

  if (!WebPPictureInit(&pic) || !WebPConfigInit(&config)) {
    fprintf(stderr, "init failed\n");
    return 2;
  }
  pic.width = width;
  pic.height = height;
  if (!WebPImportRGB565(rgb565, &pic)) {
    fprintf(stderr, "import failed\n");
    free(rgb565);
    return 1;
  }
  free(rgb565);

  WebPMemoryWriterInit(&writer);
  pic.writer = WebPMemoryWrite;
  pic.custom_ptr = &writer;
  if (!WebPEncode(&config, &pic)) {
    fprintf(stderr, "encode failed: %d\n", pic.error_code);
    WebPMemoryWriterClear(&writer);
    WebPPictureFree(&pic);
    return 1;
  }
  WebPPictureFree(&pic);

  decoded = WebPDecodeRGBA(writer.mem, writer.size, NULL, NULL);
  if (decoded != NULL) {
    WebPFree(decoded);
    ok = 1;
  }
  WebPMemoryWriterClear(&writer);
  printf("round-trip %s\n", ok ? "ok" : "failed");
  return ok ? 0 : 1;
}
