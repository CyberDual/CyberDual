#include <exiv2/exiv2.hpp>

#include <string>
#include <vector>

int main() {
  std::vector<Exiv2::byte> thumbnail(256 * 3);
  thumbnail.insert(thumbnail.end(), {'R', 'L', 'E', 0xfd, 0x00, 0x01, 0x01});

  std::string eps = "%!PS-Adobe-3.0 EPSF-3.0\n"
                    "%%BoundingBox: 0 0 1 1\n"
                    "%AI7_Thumbnail: 1 1 8\n";
  const size_t encodedSize = thumbnail.size() * 2 + ((thumbnail.size() + 31) / 32) * 2;
  eps += "%%BeginData: " + std::to_string(encodedSize) + " Hex\n";
  constexpr char hex[] = "0123456789ABCDEF";
  for (size_t i = 0; i < thumbnail.size(); ++i) {
    if (i % 32 == 0)
      eps += '%';
    eps += hex[thumbnail[i] >> 4];
    eps += hex[thumbnail[i] & 0x0f];
    if (i % 32 == 31 || i + 1 == thumbnail.size())
      eps += '\n';
  }
  eps += "%%EndData\n%%EndComments\n%%EOF\n";

  auto image = Exiv2::ImageFactory::open(reinterpret_cast<const Exiv2::byte*>(eps.data()), eps.size());
  if (!image)
    return 2;
  image->readMetadata();
  Exiv2::PreviewManager manager(*image);
  const auto previews = manager.getPreviewProperties();
  if (previews.empty())
    return 3;
  const auto preview = manager.getPreviewImage(previews.front());
  return preview.size() == 0 ? 4 : 0;
}
