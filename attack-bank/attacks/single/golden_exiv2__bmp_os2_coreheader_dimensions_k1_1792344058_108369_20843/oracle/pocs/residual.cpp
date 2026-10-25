#include <exiv2/basicio.hpp>
#include <exiv2/bmpimage.hpp>

#include <array>

int main() {
  const std::array<Exiv2::byte, 26> bitmap{
      'B',  'M',  0x1A, 0x0E, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x1A, 0x00, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x80, 0x02,
      0xE0, 0x01, 0x01, 0x00, 0x18, 0x00,
  };

  auto memIo = std::make_unique<Exiv2::MemIo>(bitmap.data(), bitmap.size());
  Exiv2::BmpImage image(std::move(memIo), Exiv2::ImageCtorParams(false, 1000));
  image.readMetadata();
  if (image.pixelWidth() != 640 || image.pixelHeight() != 480) {
    return 2;
  }
  return 0;
}
