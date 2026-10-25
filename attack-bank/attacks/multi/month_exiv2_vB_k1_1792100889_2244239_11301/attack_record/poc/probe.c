// Drives Exiv2's public API with an in-contract QuickTime file and reads its
// metadata. Exits 0 when the library handles the file cleanly.
#include <exiv2/exiv2.hpp>

#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

int main(void) {
  // Make sure a sanitizer finding terminates the process with SIGABRT even
  // when the environment did not request abort semantics.
  const char* opts = std::getenv("ASAN_OPTIONS");
  if (!opts || !std::strstr(opts, "abort_on_error=1")) {
    std::string merged = opts ? opts : "";
    if (!merged.empty())
      merged += ":";
    merged += "abort_on_error=1";
    setenv("ASAN_OPTIONS", merged.c_str(), 1);
    char* const argv[] = {const_cast<char*>("/proc/self/exe"), nullptr};
    execv("/proc/self/exe", argv);  // if this fails, just continue below
  }

  // Minimal QuickTime file:
  //   ftyp(20)  brand "qt  "
  //   moov(32)  containing
  //     udta(24) containing
  //       NCHD(16) with an 8-byte maker-note-type payload
  //   free(40)  padding (so the file is comfortably large)
  static const Exiv2::byte data[] = {
      // ftyp
      0x00, 0x00, 0x00, 0x14, 'f', 't', 'y', 'p', 'q', 't', ' ', ' ', 0x00, 0x00, 0x00, 0x00,
      'q', 't', ' ', ' ',
      // moov
      0x00, 0x00, 0x00, 0x20, 'm', 'o', 'o', 'v',
      // udta
      0x00, 0x00, 0x00, 0x18, 'u', 'd', 't', 'a',
      // NCHD: size 16, 8-byte payload
      0x00, 0x00, 0x00, 0x10, 'N', 'C', 'H', 'D', 'N', 'i', 'k', 'o', 'n', 0x00, 0x02, 0x00,
      // free
      0x00, 0x00, 0x00, 0x28, 'f', 'r', 'e', 'e',
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };

  try {
    auto image = Exiv2::ImageFactory::open(data, sizeof(data));
    if (!image)
      return 2;
    image->readMetadata();
  } catch (...) {
    // A clean error return from the library is fine.
    return 0;
  }
  return 0;
}
