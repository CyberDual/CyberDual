#include <exiv2/exiv2.hpp>

#include <cstdint>
#include <vector>

int main() {
  std::vector<uint8_t> data(101, 0);
  auto put16 = [&](size_t at, uint16_t value) {
    data[at] = static_cast<uint8_t>(value);
    data[at + 1] = static_cast<uint8_t>(value >> 8);
  };
  auto put32 = [&](size_t at, uint32_t value) {
    for (size_t i = 0; i < 4; ++i)
      data[at + i] = static_cast<uint8_t>(value >> (8 * i));
  };

  data[0] = 'I';
  data[1] = 'I';
  put16(2, 42);
  put32(4, 8);
  put16(8, 2);
  put16(10, 0x010f);
  put16(12, 2);
  put32(14, 9);
  put32(18, 38);
  put16(22, 0x8769);
  put16(24, 4);
  put32(26, 1);
  put32(30, 48);
  const char make[] = "FUJIFILM";
  for (size_t i = 0; i < sizeof(make); ++i)
    data[38 + i] = static_cast<uint8_t>(make[i]);
  put16(48, 1);
  put16(50, 0x927c);
  put16(52, 7);
  put32(54, 30);
  put32(58, 70);
  const char signature[] = "FUJIFILM";
  for (size_t i = 0; i < sizeof(signature) - 1; ++i)
    data[70 + i] = static_cast<uint8_t>(signature[i]);
  put32(78, 30);

  try {
    auto image = Exiv2::ImageFactory::open(data.data(), data.size());
    image->readMetadata();
  } catch (const Exiv2::Error&) {
  }
}
