#include <exiv2/exiv2.hpp>

#include <string>

int main() {
  try {
    // CIFF/CRW image consisting of the header and an empty root directory
    const unsigned char crw[] = {'I', 'I', 0x0e, 0x00, 0x00, 0x00, 'H', 'E', 'A', 'P',
                                 'C', 'C', 'D',  'R',  0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    auto image = Exiv2::ImageFactory::open(crw, sizeof(crw));
    image->readMetadata();

    Exiv2::ExifData& exifData = image->exifData();
    auto value = Exiv2::Value::create(Exiv2::signedShort);
    std::string numbers;
    for (int i = 0; i < 512; ++i) {
      numbers += "42 ";
    }
    value->read(numbers);
    exifData.add(Exiv2::ExifKey("Exif.CanonCs.0x01fe"), value.get());

    image->writeMetadata();
  } catch (const Exiv2::Error&) {
  }
  return 0;
}
