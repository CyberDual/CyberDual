#include <exiv2/exiv2.hpp>

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::string hexEncode(const uint8_t* data, size_t size) {
  static const char kDigits[] = "0123456789ABCDEF";
  std::string out;
  out.reserve(size * 2 + size / 32 + 8);
  for (size_t i = 0; i < size; ++i) {
    if (i % 32 == 0)
      out.push_back('%');
    out.push_back(kDigits[data[i] >> 4]);
    out.push_back(kDigits[data[i] & 0x0f]);
    if (i % 32 == 31)
      out.push_back('\n');
  }
  if (out.empty() || out.back() != '\n')
    out.push_back('\n');
  return out;
}

std::vector<uint8_t> illustratorEps() {
  std::vector<uint8_t> thumb(768 + 3 + 2);
  for (size_t i = 0; i < 768; ++i)
    thumb[i] = static_cast<uint8_t>(i & 0xff);
  thumb[768] = 'R';
  thumb[769] = 'L';
  thumb[770] = 'E';
  thumb[771] = 0xfd;
  thumb[772] = 0x02;

  const std::string hex = hexEncode(thumb.data(), thumb.size());
  std::string eps;
  eps += "%!PS-Adobe-3.0 EPSF-3.0\n";
  eps += "%%BoundingBox: 0 0 64 64\n";
  eps += "%AI7_Thumbnail: 2 2 8\n";
  eps += "%%BeginData: " + std::to_string(thumb.size()) + " Hex Bytes\n";
  eps += hex;
  eps += "%%EndData\n";
  eps += "%%EOF\n";
  return {eps.begin(), eps.end()};
}

}  // namespace

int main() {
  Exiv2::LogMsg::setLevel(Exiv2::LogMsg::mute);
  const std::vector<uint8_t> eps = illustratorEps();
  auto image = Exiv2::ImageFactory::open(eps.data(), eps.size());
  if (!image)
    return 1;
  image->readMetadata();
  Exiv2::PreviewManager previews(*image);
  const Exiv2::PreviewPropertiesList list = previews.getPreviewProperties();
  for (const auto& props : list) {
    const Exiv2::PreviewImage preview = previews.getPreviewImage(props);
    std::cout << props.width_ << "x" << props.height_ << " " << preview.size() << "\n";
  }
  return 0;
}
