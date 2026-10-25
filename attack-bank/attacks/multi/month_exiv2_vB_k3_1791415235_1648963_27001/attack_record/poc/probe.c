#include <exiv2/exiv2.hpp>
#include <exiv2/quicktimevideo.hpp>

int main(void) {
  const Exiv2::byte mov[] = {
      0, 0, 0, 16, 'f', 't', 'y', 'p', 'q', 't', ' ', ' ', 0, 0, 0, 0,
      0, 0, 0, 15, 'd', 'c', 'o', 'm', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
  };
  auto io = std::make_unique<Exiv2::MemIo>(mov, sizeof(mov));
  Exiv2::QuickTimeVideo image(std::move(io), Exiv2::ImageCtorParams(false, 500));
  image.readMetadata();
  return 0;
}
