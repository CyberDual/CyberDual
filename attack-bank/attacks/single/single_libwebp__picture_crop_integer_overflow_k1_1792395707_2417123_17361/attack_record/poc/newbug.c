// One-probe regression test: craft a WebP bitstream through the public
// encoder, decode it back through the public decoder, and exercise the
// public WebPPictureCrop() API on the imported picture.
#include <stdio.h>
#include <stdlib.h>

#include <webp/decode.h>
#include <webp/encode.h>

int main(void) {
  const int kWidth = 100;
  const int kHeight = 50;
  const int kCropLeft = 10;
  const int kCropTop = 90;
  const int kCropWidth = 10;
  const int kCropHeight = 10;
  uint8_t* rgb = NULL;
  uint8_t* webp = NULL;
  uint8_t* decoded = NULL;
  size_t webp_size;
  int w = 0, h = 0, i;
  WebPPicture pic;
  int ok = 0;

  rgb = (uint8_t*)malloc((size_t)kWidth * kHeight * 3);
  if (rgb == NULL) return 2;
  for (i = 0; i < kWidth * kHeight * 3; ++i) {
    rgb[i] = (uint8_t)(((uint32_t)i * 2654435761u) >> 13);
  }

  webp_size = WebPEncodeRGB(rgb, kWidth, kHeight, kWidth * 3, 75.f, &webp);
  if (webp_size == 0 || webp == NULL) {
    free(rgb);
    return 2;
  }

  decoded = WebPDecodeRGB(webp, webp_size, &w, &h);
  if (decoded == NULL || w != kWidth || h != kHeight) {
    free(rgb);
    WebPFree(webp);
    return 2;
  }

  WebPPictureInit(&pic);
  pic.width = w;
  pic.height = h;
  pic.use_argb = 0;
  if (!WebPPictureImportRGB(&pic, decoded, w * 3)) {
    free(rgb);
    WebPFree(webp);
    WebPFree(decoded);
    return 2;
  }

  ok = WebPPictureCrop(&pic, kCropLeft, kCropTop, kCropWidth, kCropHeight);
  fprintf(stderr, "crop(left=%d, top=%d, %dx%d) on %dx%d picture: %s\n",
          kCropLeft, kCropTop, kCropWidth, kCropHeight, w, h,
          ok ? "accepted" : "rejected");

  WebPPictureFree(&pic);
  free(rgb);
  WebPFree(webp);
  WebPFree(decoded);
  return 0;
}
