#include <exiv2/exiv2.hpp>

#include <cstdint>
#include <exception>
#include <string>
#include <vector>

namespace {

std::string hex_line(const std::string& digits) {
  std::string out;
  for (size_t i = 0; i < digits.size(); i += 64) {
    out.push_back('%');
    out.append(digits, i, 64);
    out.push_back('\n');
  }
  return out;
}

std::vector<Exiv2::byte> make_eps() {
  // 1x1 AI7 thumbnail. The palette is 256 RGB entries, followed by an RLE
  // stream whose first run uses the extended (zero-count) repeat form.
  std::string table(256 * 3 * 2, '0');
  const std::string image = "524C45FD005555555600";

  std::string eps;
  eps += "%!PS-Adobe-3.0 EPSF-3.0\n";
  eps += "%AI7_Thumbnail: 1 1 8\n";
  eps += "%%BeginData: 1 Hex\n";
  eps += hex_line(table);
  eps += hex_line(image);
  eps += "%%EndData\n";
  eps += "%%EOF\n";

  return std::vector<Exiv2::byte>(eps.begin(), eps.end());
}

}  // namespace

int main(void) {
  try {
    const std::vector<Exiv2::byte> data = make_eps();
    Exiv2::Image::UniquePtr image = Exiv2::ImageFactory::open(data.data(), data.size());
    if (!image)
      return 0;
    image->readMetadata();
    Exiv2::PreviewManager previews(*image);
    const Exiv2::PreviewPropertiesList list = previews.getPreviewProperties();
    if (!list.empty()) {
      const Exiv2::PreviewImage preview = previews.getPreviewImage(list.front());
      if (preview.pData() == nullptr && preview.size() != 0)
        return 2;
    }
  } catch (const Exiv2::Error&) {
    return 0;
  } catch (const std::exception&) {
    return 0;
  }
  return 0;
}
