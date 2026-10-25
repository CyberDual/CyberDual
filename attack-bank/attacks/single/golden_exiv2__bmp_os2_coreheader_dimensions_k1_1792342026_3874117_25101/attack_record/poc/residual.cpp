// SPDX-License-Identifier: GPL-2.0-or-later
//
// Builds a Windows bitmap in memory from its bytes -- a plain 16x16 image whose DIB header is
// the 124-byte BITMAPV5HEADER -- and reads it through the public Exiv2 entry point, printing
// the pixel dimensions the library reports for it.

#include <exiv2/exiv2.hpp>

#include <array>
#include <cstdint>
#include <cstdio>

namespace {
void put16(unsigned char* p, std::uint16_t v) {
  p[0] = static_cast<unsigned char>(v & 0xFF);
  p[1] = static_cast<unsigned char>((v >> 8) & 0xFF);
}

void put32(unsigned char* p, std::uint32_t v) {
  p[0] = static_cast<unsigned char>(v & 0xFF);
  p[1] = static_cast<unsigned char>((v >> 8) & 0xFF);
  p[2] = static_cast<unsigned char>((v >> 16) & 0xFF);
  p[3] = static_cast<unsigned char>((v >> 24) & 0xFF);
}
}  // namespace

int main() {
  // 14-byte BITMAPFILEHEADER, 124-byte BITMAPV5HEADER, then 16 rows of 16 24-bit pixels.
  std::array<unsigned char, 14 + 124 + 16 * 16 * 3> file{};

  file[0] = 'B';
  file[1] = 'M';
  put32(&file[2], static_cast<std::uint32_t>(file.size()));  // file size in bytes
  put32(&file[10], 14 + 124);                                // offset of the bitmap data

  put32(&file[14], 124);         // header size: BITMAPV5HEADER
  put32(&file[18], 16);          // width in pixels
  put32(&file[22], 16);          // height in pixels
  put16(&file[26], 1);           // plane count
  put16(&file[28], 24);          // depth
  put32(&file[30], 0);           // compression: BI_RGB
  put32(&file[34], 16 * 16 * 3);  // image size in bytes
  put32(&file[54], 0x00FF0000);  // red mask
  put32(&file[58], 0x0000FF00);  // green mask
  put32(&file[62], 0x000000FF);  // blue mask
  put32(&file[66], 0xFF000000);  // alpha mask
  put32(&file[70], 0x73524742);  // colour space: LCS_sRGB
  put32(&file[118], 4);          // intent: LCS_GM_IMAGES

  auto image = Exiv2::ImageFactory::open(file.data(), file.size());
  image->readMetadata();
  std::printf("%d x %d\n", image->pixelWidth(), image->pixelHeight());
  return 0;
}
