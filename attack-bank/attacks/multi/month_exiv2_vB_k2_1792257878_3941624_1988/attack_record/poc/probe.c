#include <exiv2/exiv2.hpp>

#include <array>
#include <cstddef>
#include <exception>

int main(void) {
  std::array<unsigned char, 1024> data{};

  data[3] = 16;
  data[4] = 'f';
  data[5] = 't';
  data[6] = 'y';
  data[7] = 'p';
  data[8] = 'q';
  data[9] = 't';
  data[10] = ' ';
  data[11] = ' ';
  data[19] = 16;
  data[20] = 's';
  data[21] = 'm';
  data[22] = 'h';
  data[23] = 'd';

  try {
    auto image = Exiv2::ImageFactory::open(data.data(), data.size());
    image->readMetadata();
  } catch (const std::exception&) {
  }
}
