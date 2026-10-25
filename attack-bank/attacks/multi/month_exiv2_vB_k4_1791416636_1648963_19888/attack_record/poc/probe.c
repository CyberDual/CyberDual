#include <exiv2/exiv2.hpp>

#include <cstdio>
#include <string>

int main(void) {
  std::string eps = "%!PS-Adobe-3.0 EPSF-3.0\n"
                    "%%BoundingBox: 0 0 1 1\n"
                    "%AI7_Thumbnail: 1 1 8\n"
                    "%%BeginData: 1540 Hex Bytes\n"
                    "%";
  eps.append(256 * 3 * 2, '0');
  eps += "00\n%%EndData\n%%EndComments\n%%EOF\n";

  try {
    auto image = Exiv2::ImageFactory::open(
        reinterpret_cast<const Exiv2::byte*>(eps.data()), eps.size());
    if (!image)
      return 2;
    image->readMetadata();
    Exiv2::PreviewManager previews(*image);
    auto properties = previews.getPreviewProperties();
    if (properties.empty())
      return 3;
    auto preview = previews.getPreviewImage(properties.front());
    return preview.size() ? 0 : 4;
  } catch (const Exiv2::Error& error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 5;
  }
}
