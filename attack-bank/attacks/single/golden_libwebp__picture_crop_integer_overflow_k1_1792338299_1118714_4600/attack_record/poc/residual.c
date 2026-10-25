// Drives WebPPictureCrop (public API, <webp/encode.h>) with a crafted crop
// rectangle over an ordinary WebP image round-tripped through
// WebPEncodeRGB/WebPDecodeRGB.
#include <stdint.h>
#include <stdio.h>
#include <webp/decode.h>
#include <webp/encode.h>

int main(void) {
  enum { W = 64, H = 64 };
  uint8_t rgb[W * H * 3];
  for (int i = 0; i < W * H * 3; ++i) rgb[i] = (uint8_t)(i * 31 + 7);

  uint8_t* webp_data = NULL;
  const size_t webp_size = WebPEncodeRGB(rgb, W, H, W * 3, 90.f, &webp_data);
  if (webp_size == 0) return 2;

  int dw = 0, dh = 0;
  uint8_t* const decoded = WebPDecodeRGB(webp_data, webp_size, &dw, &dh);
  WebPFree(webp_data);
  if (decoded == NULL || dw != W || dh != H) return 2;

  WebPPicture pic;
  if (!WebPPictureInit(&pic)) return 2;
  pic.width = dw;
  pic.height = dh;
  pic.use_argb = 0;
  if (!WebPPictureImportRGB(&pic, decoded, dw * 3)) return 2;
  WebPFree(decoded);

  const int left = 2147483584;  // 0x7fffffc0
  const int top = 0;
  if (!WebPPictureCrop(&pic, left, top, W, H)) {
    puts("crop rejected");
    WebPPictureFree(&pic);
    return 0;
  }
  printf("crop accepted: %dx%d\n", pic.width, pic.height);
  WebPPictureFree(&pic);
  return 0;
}
