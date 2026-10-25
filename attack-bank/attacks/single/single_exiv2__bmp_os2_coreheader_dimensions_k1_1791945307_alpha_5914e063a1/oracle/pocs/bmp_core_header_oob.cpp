// PoC for the BITMAPCOREHEADER handling in Exiv2::BmpImage::readMetadata.
//
// The input is a well-formed OS/2 1.x / Windows 2.x bitmap: a 14-byte
// BITMAPFILEHEADER followed by a 12-byte BITMAPCOREHEADER whose width and
// height are 16-bit fields, followed by 24-bpp pixel data.
//
// Expected result:
//   * patched tree, ASan build: stack-buffer-overflow abort (exit 134)
//   * parent tree,  ASan build: parses without a memory error (exit 0)

#include <exiv2/exiv2.hpp>

#include <cstdio>
#include <vector>

int main() {
  constexpr unsigned width = 8;
  constexpr unsigned height = 8;
  constexpr size_t rowSize = ((width * 24 + 31) / 32) * 4;  // 24-bpp rows, DWORD padded
  constexpr size_t pixelDataSize = rowSize * height;
  constexpr size_t fileSize = 14 + 12 + pixelDataSize;

  std::vector<unsigned char> bmp{
      'B',  'M',               // signature
      0,    0,    0,    0,     // file size (filled in below)
      0,    0,    0,    0,     // reserved
      26,   0,    0,    0,     // offset of the pixel data
      12,   0,    0,    0,     // size of the BITMAPCOREHEADER
      8,    0,               // width in pixels (16-bit)
      8,    0,               // height in pixels (16-bit)
      1,    0,               // colour planes
      24,   0,               // bits per pixel
  };
  bmp.resize(fileSize, 0xAB);  // pixel data
  bmp[2] = static_cast<unsigned char>(fileSize);

  try {
    auto image = Exiv2::ImageFactory::open(bmp.data(), bmp.size());
    image->readMetadata();
    std::printf("readMetadata completed: %ux%u\n", image->pixelWidth(), image->pixelHeight());
  } catch (const Exiv2::Error& e) {
    std::printf("exiv2 error: %s\n", e.what());
  }
  return 0;
}
