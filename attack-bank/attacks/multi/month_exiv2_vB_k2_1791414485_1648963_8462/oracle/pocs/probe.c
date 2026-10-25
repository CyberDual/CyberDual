#include <exiv2/exiv2.hpp>

#include <cstdio>
#include <string>
#include <vector>

int main(void) {
  constexpr size_t width = 19964;
  constexpr size_t height = 4482;
  constexpr size_t pixels = width * height;
  const size_t runs = pixels / 255;
  const size_t remainder = pixels % 255;

  std::vector<unsigned char> thumbnail(256 * 3, 0);
  thumbnail.push_back('R');
  thumbnail.push_back('L');
  thumbnail.push_back('E');
  for (size_t i = 0; i < runs; ++i) {
    thumbnail.push_back(0xfd);
    thumbnail.push_back(255);
    thumbnail.push_back(0);
  }
  if (remainder != 0) {
    thumbnail.push_back(0xfd);
    thumbnail.push_back(static_cast<unsigned char>(remainder));
    thumbnail.push_back(0);
  }

  constexpr char hex[] = "0123456789ABCDEF";
  std::string encoded;
  encoded.push_back('%');
  size_t lineBytes = 0;
  for (unsigned char value : thumbnail) {
    encoded.push_back(hex[value >> 4]);
    encoded.push_back(hex[value & 15]);
    if (++lineBytes == 32) {
      encoded += "\n%";
      lineBytes = 0;
    }
  }
  encoded.push_back('\n');
  std::string eps = "%!PS-Adobe-3.0 EPSF-3.0\n"
                    "%%BoundingBox: 0 0 100 100\n"
                    "%AI7_Thumbnail: 19964 4482 8\n"
                    "%%BeginData: " + std::to_string(encoded.size()) + " Hex Bytes\n" +
                    encoded + "%%EndData\n%%EndComments\n%%EOF\n";

  try {
    auto image = Exiv2::ImageFactory::open(reinterpret_cast<const Exiv2::byte*>(eps.data()), eps.size());
    if (!image)
      return 2;
    image->readMetadata();
    Exiv2::PreviewManager manager(*image);
    auto previews = manager.getPreviewProperties();
    std::printf("%zu\n", previews.size());
  } catch (const Exiv2::Error& error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 3;
  }
  return 0;
}
