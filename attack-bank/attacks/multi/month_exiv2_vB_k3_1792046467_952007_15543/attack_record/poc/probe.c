// SPDX-License-Identifier: GPL-2.0-or-later
//
// Probe for the QuickTime/MOV metadata reader (QuickTimeVideo::readMetadata).
//
// The file below is a valid QuickTime container:
//
//   ftyp  - brand "qt  ", minor version 0, one compatible brand
//   dcom  - compression identifier atom carrying a 64-byte payload
//   udta  - user data atom holding a single " cmt" (comment) sub-atom with a
//           200-byte string payload
//
// Everything is in-contract input for Exiv2::ImageFactory::open() +
// Image::readMetadata(): atom sizes match the actual payload lengths and the
// whole file is consumed exactly.

#include <exiv2/exiv2.hpp>

#include <cstdint>
#include <cstdio>
#include <vector>

// Report sanitizer findings by aborting (SIGABRT) rather than exiting, so the
// result is visible to wrappers that only check for signal death.
extern "C" const char* __asan_default_options() {
  return "abort_on_error=1:detect_leaks=0";
}

namespace {

void be32(std::vector<uint8_t>& out, uint32_t v) {
  out.push_back(static_cast<uint8_t>(v >> 24));
  out.push_back(static_cast<uint8_t>(v >> 16));
  out.push_back(static_cast<uint8_t>(v >> 8));
  out.push_back(static_cast<uint8_t>(v));
}

void tag(std::vector<uint8_t>& out, const char (&t)[5]) {
  for (int i = 0; i < 4; ++i)
    out.push_back(static_cast<uint8_t>(t[i]));
}

std::vector<uint8_t> makeFile() {
  std::vector<uint8_t> f;

  // ftyp: 8 byte header + 12 byte payload (major brand, minor version, brand)
  be32(f, 20);
  tag(f, "ftyp");
  tag(f, "qt  ");
  be32(f, 0);
  tag(f, "qt  ");

  // udta: 8 byte header + one " cmt" sub-atom of 212 bytes total
  //       (8 byte sub-atom header + 4 byte language tag + 200 byte string)
  const size_t cmtSize = 212;
  be32(f, 8 + cmtSize);
  tag(f, "udta");
  be32(f, static_cast<uint32_t>(cmtSize));
  tag(f, " cmt");
  be32(f, 0);
  for (int i = 0; i < 200; ++i)
    f.push_back(static_cast<uint8_t>('A' + (i % 26)));

  // dcom: 8 byte header + 64 byte compression identifier string
  be32(f, 8 + 64);
  tag(f, "dcom");
  for (int i = 0; i < 64; ++i)
    f.push_back(static_cast<uint8_t>('a' + (i % 26)));

  return f;
}

}  // namespace

int main(void) {
  const std::vector<uint8_t> file = makeFile();
  std::printf("probe: parsing %zu byte QuickTime stream\n", file.size());

  Exiv2::LogMsg::setLevel(Exiv2::LogMsg::mute);
  try {
    Exiv2::Image::UniquePtr image = Exiv2::ImageFactory::open(file.data(), file.size());
    if (!image) {
      std::printf("probe: no image\n");
      return 0;
    }
    image->readMetadata();
    const std::string& compressor = image->xmpData()["Xmp.video.Compressor"].toString();
    const std::string& comment = image->xmpData()["Xmp.video.Comment"].toString();
    std::printf("probe: compressor=%s comment=%zu bytes\n", compressor.c_str(), comment.size());
  } catch (const std::exception& e) {
    std::printf("probe: caught exception: %s\n", e.what());
  }
  std::printf("probe: done\n");
  return 0;
}
