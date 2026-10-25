#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "webp/decode.h"

int main(void) {
  static const uint8_t image[] = {
      0x2f, 0x07, 0x00, 0x00, 0x00, 0x84, 0x19,
      0x88, 0x88, 0x28, 0x22, 0x22, 0x00,
  };
  int width = 0;
  int height = 0;
  uint8_t* const rgba = WebPDecodeRGBA(image, sizeof(image), &width, &height);
  if (rgba != NULL) {
    WebPFree(rgba);
    printf("decoded %d x %d\n", width, height);
  } else {
    puts("decode rejected");
  }
  return 0;
}
