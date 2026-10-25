#include <exiv2/exiv2.hpp>

#include <string>

int main(void) {
  try {
    std::string imageHex(1536, '0');
    imageHex += "524C45";
    for (int i = 0; i < 7; ++i) {
      imageHex += "FDFF00";
    }
    imageHex.insert(imageHex.begin(), '%');

    const std::string eps = "%!PS-Adobe-3.0 EPSF-3.0\n"
                            "%%BoundingBox: 0 0 1 1\n"
                            "%AI7_Thumbnail: 1024 1 8\n"
                            "%%BeginData: 1584 Hex\n" +
                            imageHex +
                            "\n%%EndData\n"
                            "%%EndComments\n"
                            "%%EOF\n";

    auto image = Exiv2::ImageFactory::open(reinterpret_cast<const Exiv2::byte*>(eps.data()), eps.size());
    image->readMetadata();

    const Exiv2::PreviewManager previews(*image);
    const auto properties = previews.getPreviewProperties();
    if (properties.empty()) {
      return 2;
    }

    const auto preview = previews.getPreviewImage(properties.front());
    const auto data = preview.copy();
    return data.empty() ? 3 : 0;
  } catch (const Exiv2::Error&) {
    return 4;
  }
}
