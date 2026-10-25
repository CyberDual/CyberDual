#include <exiv2/exiv2.hpp>

#include <iterator>
#include <vector>

int main(void) {
  const unsigned char quicktime_file[] = {
      0x00, 0x00, 0x00, 0x14, 'f',  't',  'y',  'p',  'q',  't',  ' ',  ' ',
      0x00, 0x00, 0x02, 0x00, 'q',  't',  ' ',  ' ',  0x00, 0x00, 0x00, 0x10,
      'm',  'e',  't',  'a',  0x00, 0x00, 0x00, 0x08, 'f',  'r',  'e',  'e',
      0x00, 0x00, 0x20, 0x00, 'f',  'r',  'e',  'e'};

  std::vector<unsigned char> input(std::begin(quicktime_file), std::end(quicktime_file));
  input.resize(input.size() + 8184);

  auto io = std::make_unique<Exiv2::MemIo>(input.data(), input.size());
  auto image = Exiv2::ImageFactory::open(std::move(io));
  if (!image)
    return 1;
  image->readMetadata();
  return 0;
}
