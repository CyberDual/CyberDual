#include <exiv2/exiv2.hpp>

#include <string>

// Reproducer for writing a Canon Camera Settings array to a CRW image.
int main() {
  static const Exiv2::byte crw[] = {
      0x49, 0x49, 0x0e, 0x00, 0x00, 0x00, 0x48, 0x45, 0x41, 0x50,
      0x43, 0x43, 0x44, 0x52, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };

  try {
    Exiv2::Image::UniquePtr image = Exiv2::ImageFactory::open(crw, sizeof(crw));
    image->readMetadata();

    auto value = Exiv2::Value::create(Exiv2::signedShort);
    std::string components = "42";
    for (int i = 1; i < 600; ++i) {
      components += " 42";
    }
    value->read(components);
    image->exifData().add(Exiv2::ExifKey("Exif.CanonCs.Macro"), value.get());

    image->writeMetadata();
  } catch (const Exiv2::Error&) {
    return 0;
  }
  return 0;
}
