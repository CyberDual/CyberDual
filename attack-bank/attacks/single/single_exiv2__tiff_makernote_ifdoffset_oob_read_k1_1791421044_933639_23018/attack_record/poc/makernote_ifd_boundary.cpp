#include <exiv2/exiv2.hpp>

#include <array>
#include <cstdint>

int main() {
  std::array<Exiv2::byte, 92> data{};
  auto put16 = [&](size_t pos, uint16_t value) {
    data[pos] = static_cast<Exiv2::byte>(value);
    data[pos + 1] = static_cast<Exiv2::byte>(value >> 8);
  };
  auto put32 = [&](size_t pos, uint32_t value) {
    data[pos] = static_cast<Exiv2::byte>(value);
    data[pos + 1] = static_cast<Exiv2::byte>(value >> 8);
    data[pos + 2] = static_cast<Exiv2::byte>(value >> 16);
    data[pos + 3] = static_cast<Exiv2::byte>(value >> 24);
  };

  data[0] = 'I';
  data[1] = 'I';
  put16(2, 42);
  put32(4, 8);
  put16(8, 2);
  put16(10, 0x010f);
  put16(12, 2);
  put32(14, 5);
  put32(18, 38);
  put16(22, 0x8769);
  put16(24, 4);
  put32(26, 1);
  put32(30, 44);
  data[38] = 'F';
  data[39] = 'U';
  data[40] = 'J';
  data[41] = 'I';
  put16(44, 1);
  put16(46, 0x927c);
  put16(48, 7);
  put32(50, 30);
  put32(54, 62);
  constexpr std::array<Exiv2::byte, 8> signature = {'F', 'U', 'J', 'I', 'F', 'I', 'L', 'M'};
  for (size_t i = 0; i < signature.size(); ++i)
    data[62 + i] = signature[i];
  put32(70, 29);

  try {
    auto image = Exiv2::ImageFactory::open(data.data(), data.size());
    image->readMetadata();
  } catch (const Exiv2::Error&) {
  }
}
