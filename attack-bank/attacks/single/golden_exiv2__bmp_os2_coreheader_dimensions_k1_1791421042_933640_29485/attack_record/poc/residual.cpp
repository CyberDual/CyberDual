#include <exiv2/basicio.hpp>
#include <exiv2/image.hpp>

#include <cstdint>
#include <memory>
#include <vector>

int main() {
  std::vector<std::uint8_t> bmp(794);
  const std::uint8_t header[] = {
      'B', 'M', 0x1A, 0x03, 0, 0, 0, 0, 0, 0, 26, 0, 0, 0,
      12, 0, 0, 0, 16, 0, 16, 0, 1, 0, 24, 0,
  };
  for (std::size_t i = 0; i < sizeof(header); ++i) {
    bmp[i] = header[i];
  }
  auto io = std::make_unique<Exiv2::MemIo>(bmp.data(), bmp.size());
  auto image = Exiv2::ImageFactory::open(std::move(io));
  if (!image) {
    return 2;
  }
  image->readMetadata();
  return image->pixelWidth() == 16 && image->pixelHeight() == 16 ? 0 : 1;
}
