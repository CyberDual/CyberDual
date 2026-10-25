#include <stdlib.h>
#include <string.h>

#include "webp/encode.h"
#include "webp/mux.h"

static int AbortHook(int percent, const WebPPicture* data) {
  (void)percent;
  (void)data;
  return 0;
}

int main(void) {
  const int width = 16;
  const int height = 16;
  WebPAnimEncoder* enc;
  WebPPicture pic;

  enc = WebPAnimEncoderNew(width, height, NULL);
  if (enc == NULL) return 1;

  if (!WebPPictureInit(&pic)) return 1;
  pic.width = width;
  pic.height = height;
  pic.use_argb = 1;
  if (!WebPPictureAlloc(&pic)) return 1;
  memset(pic.argb, 0, (size_t)pic.argb_stride * height * sizeof(uint32_t));
  pic.progress_hook = AbortHook;

  WebPAnimEncoderAdd(enc, &pic, 0, NULL);

  WebPPictureFree(&pic);
  WebPAnimEncoderDelete(enc);
  return 0;
}
