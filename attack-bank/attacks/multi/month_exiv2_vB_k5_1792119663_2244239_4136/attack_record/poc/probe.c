// Drives the Exiv2 public API with an in-memory QuickTime file whose 'dcom'
// box declares a size that runs past the end of the buffer, then reads the
// file's metadata. Clean error handling (catch, return 0) is expected on a
// correct library; a memory-safety fault aborts under AddressSanitizer.
#include <exiv2/exiv2.hpp>

#include <cstring>
#include <exception>

extern "C" const char* __asan_default_options() {
  return "abort_on_error=1";
}

int main(void) {
  // Minimal QuickTime structure:
  //   box 1: size=24, type "ftyp", brand "qt  " (+ 12 bytes payload)
  //   box 2: size=0x400, type "dcom" -- but the buffer ends long before
  //          the declared payload size is reached.
  static const Exiv2::byte header[] = {
      0x00, 0x00, 0x00, 0x18, 'f',  't',  'y',  'p',  'q',  't',  ' ',  ' ',
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x04, 0x00, 'd',  'c',  'o',  'm',
      'X',  'X',  'X',  'X',  'X',  'X',  'X',  'X',  'X',  'X',  'X',  'X',
  };
  const size_t size = 256;
  auto* data = new Exiv2::byte[size];
  std::memset(data, 'X', size);
  std::memcpy(data, header, sizeof(header));

  try {
    auto image = Exiv2::ImageFactory::open(data, size);
    if (image)
      image->readMetadata();
  } catch (const Exiv2::Error&) {
    delete[] data;
    return 0;
  } catch (const std::exception&) {
    delete[] data;
    return 0;
  }
  delete[] data;
  return 0;
}
