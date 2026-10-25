#include <exiv2/exiv2.hpp>

#include <cstddef>
#include <string>

namespace {

void appendHexByte(std::string& destination, unsigned char value) {
  static const char digits[] = "0123456789ABCDEF";
  destination.push_back(digits[value >> 4]);
  destination.push_back(digits[value & 0x0f]);
}

}  // namespace

int main(void) {
  std::string input =
      "%!PS-Adobe-3.0 EPSF-3.0\n"
      "%AI7_Thumbnail: 1 3 8\n"
      "%%BeginData: 000000 Hex\n";

  input.push_back('%');

  for (std::size_t index = 0; index < 771; ++index) {
    appendHexByte(input, static_cast<unsigned char>(index & 0xff));
  }

  input +=
      "\n%%EndData\n"
      "%%EOF\n";

  auto image = Exiv2::ImageFactory::open(reinterpret_cast<const Exiv2::byte*>(input.data()), input.size());
  image->readMetadata();

  const Exiv2::PreviewManager previewManager(*image);
  const auto previews = previewManager.getPreviewProperties();
  if (previews.size() != 1) {
    return 1;
  }

  const auto preview = previewManager.getPreviewImage(previews[0]);
  return preview.pData() != nullptr ? 0 : 1;
}
