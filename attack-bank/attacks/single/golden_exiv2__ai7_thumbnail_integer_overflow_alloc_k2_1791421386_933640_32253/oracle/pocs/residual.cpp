#include <exiv2/exiv2.hpp>

#include <string>

int main() {
  std::string thumbnail(256 * 3, '\0');
  thumbnail += "RLE";
  thumbnail.push_back(static_cast<char>(0xfd));
  thumbnail.push_back(2);
  thumbnail.push_back(0);

  static constexpr char hex[] = "0123456789abcdef";
  std::string encoded;
  encoded.reserve(thumbnail.size() * 2);
  for (unsigned char byte : thumbnail) {
    encoded.push_back(hex[byte >> 4]);
    encoded.push_back(hex[byte & 15]);
  }

  std::string eps = "%!PS-Adobe-3.0 EPSF-3.0\n"
                    "%%BoundingBox: 0 0 1 1\n"
                    "%AI7_Thumbnail: 1 1 8\n"
                    "%%BeginData: " +
                    std::to_string(encoded.size() + 1) + " Hex\n%" + encoded +
                    "\n%%EndData\n"
                    "%%EndComments\n"
                    "%%Page: 1 1\n"
                    "showpage\n"
                    "%%EOF\n";

  auto image = Exiv2::ImageFactory::open(reinterpret_cast<const Exiv2::byte*>(eps.data()), eps.size());
  image->readMetadata();
  Exiv2::PreviewManager previews(*image);
  for (const auto& properties : previews.getPreviewProperties())
    (void)previews.getPreviewImage(properties);
}
