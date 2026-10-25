// Drive Image::writeMetadata on a Canon CRW with an in-range CanonCs value
// whose packed length wraps a uint16_t once the tag offset is included.
#include <exiv2/exiv2.hpp>

#include <cstdint>
#include <iostream>
#include <vector>

int main() {
  // Minimal CIFF/CRW header (little-endian, HEAPCCDR signature, empty heap).
  const unsigned char crw[] = {
      0x49, 0x49, 0x0e, 0x00, 0x00, 0x00, 0x48, 0x45, 0x41, 0x50, 0x43, 0x43, 0x44, 0x52, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00,
  };

  try {
    auto image = Exiv2::ImageFactory::open(crw, sizeof(crw));
    if (!image) {
      std::cerr << "open failed\n";
      return 2;
    }
    image->readMetadata();

    // Exif.CanonCs.Macro is tag 0x0001 (offset 2). 32767 signed shorts are
    // 65534 bytes, which still fits in a uint16_t by itself.
    constexpr size_t kBytes = 65534;
    std::vector<Exiv2::byte> raw(kBytes, 0x2a);
    auto value = Exiv2::Value::create(Exiv2::signedShort);
    if (value->read(raw.data(), raw.size(), Exiv2::littleEndian) != 0) {
      std::cerr << "value read failed\n";
      return 2;
    }
    image->exifData()["Exif.CanonCs.Macro"] = *value;
    image->writeMetadata();
    std::cout << "write completed\n";
    return 0;
  } catch (const Exiv2::Error& e) {
    std::cerr << "Exiv2 exception: " << e.what() << "\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "exception: " << e.what() << "\n";
    return 3;
  }
}
