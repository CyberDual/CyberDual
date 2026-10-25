#include <exiv2/exiv2.hpp>

#include <cstdint>
#include <iostream>
#include <vector>

namespace {

void append16(std::vector<Exiv2::byte>& out, uint16_t value) {
  out.push_back(static_cast<Exiv2::byte>(value & 0xff));
  out.push_back(static_cast<Exiv2::byte>((value >> 8) & 0xff));
}

void append32(std::vector<Exiv2::byte>& out, uint32_t value) {
  out.push_back(static_cast<Exiv2::byte>(value & 0xff));
  out.push_back(static_cast<Exiv2::byte>((value >> 8) & 0xff));
  out.push_back(static_cast<Exiv2::byte>((value >> 16) & 0xff));
  out.push_back(static_cast<Exiv2::byte>((value >> 24) & 0xff));
}

std::vector<Exiv2::byte> coreBitmap() {
  constexpr uint32_t kOffBits = 26;
  constexpr uint32_t kPixelPrefix = 9000;
  constexpr uint32_t kFileSize = kOffBits + kPixelPrefix;

  std::vector<Exiv2::byte> bmp;
  bmp.reserve(kFileSize);
  bmp.push_back('B');
  bmp.push_back('M');
  append32(bmp, kFileSize);
  append16(bmp, 0);
  append16(bmp, 0);
  append32(bmp, kOffBits);
  append32(bmp, 12);
  append16(bmp, 21925);
  append16(bmp, 65297);
  append16(bmp, 1);
  append16(bmp, 24);
  bmp.resize(kFileSize, 0x5a);
  return bmp;
}

}  // namespace

int main() {
  const std::vector<Exiv2::byte> bmp = coreBitmap();
  Exiv2::Image::UniquePtr image = Exiv2::ImageFactory::open(bmp.data(), bmp.size());
  if (!image) {
    std::cerr << "open failed\n";
    return 1;
  }
  image->readMetadata();
  std::cout << image->pixelWidth() << " " << image->pixelHeight() << "\n";
  return 0;
}
