#include <exiv2/exiv2.hpp>

#include <cstdint>
#include <exception>
#include <string>

int main(void) {
  try {
    Exiv2::LogMsg::setLevel(Exiv2::LogMsg::mute);

    constexpr unsigned width = 65536;
    constexpr unsigned height = 1;
    const std::string raw(256 * 3 + static_cast<size_t>(width) * height, '\0');

    std::string hex;
    hex.reserve(raw.size() * 2);
    static const char kDigits[] = "0123456789abcdef";
    for (unsigned char c : raw) {
      hex.push_back(kDigits[c >> 4]);
      hex.push_back(kDigits[c & 0x0f]);
    }

    std::string eps;
    eps.reserve(hex.size() + 128);
    eps += "%!PS-Adobe-3.0 EPSF-3.0\n";
    eps += "%%BoundingBox: 0 0 1 1\n";
    eps += "%AI7_Thumbnail: 65536 1 8\n";
    eps += "%%BeginData: ";
    eps += std::to_string(hex.size() + 1);
    eps += " Hex Bytes\n%";
    eps += hex;
    eps += "\n%%EndData\n%%EndComments\n%%EOF\n";

    auto image = Exiv2::ImageFactory::open(reinterpret_cast<const Exiv2::byte*>(eps.data()), eps.size());
    if (!image)
      return 0;
    image->readMetadata();
    Exiv2::PreviewManager manager(*image);
    auto props = manager.getPreviewProperties();
    (void)props;
  } catch (const Exiv2::Error&) {
    return 0;
  } catch (const std::exception&) {
    return 0;
  }
  return 0;
}
