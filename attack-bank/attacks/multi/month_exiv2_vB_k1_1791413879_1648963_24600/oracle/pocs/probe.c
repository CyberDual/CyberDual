#include <exiv2/exiv2.hpp>

#include <cstdint>
#include <vector>

static void append32(std::vector<Exiv2::byte>& out, uint32_t value) {
  out.push_back(static_cast<Exiv2::byte>(value >> 24));
  out.push_back(static_cast<Exiv2::byte>(value >> 16));
  out.push_back(static_cast<Exiv2::byte>(value >> 8));
  out.push_back(static_cast<Exiv2::byte>(value));
}

static void appendBox(std::vector<Exiv2::byte>& out, const char* type,
                      const std::vector<Exiv2::byte>& body) {
  append32(out, static_cast<uint32_t>(body.size() + 8));
  for (int i = 0; i < 4; ++i)
    out.push_back(static_cast<Exiv2::byte>(type[i]));
  out.insert(out.end(), body.begin(), body.end());
}

int main(void) {
  std::vector<Exiv2::byte> data;
  const std::vector<Exiv2::byte> brands = {'q', 't', ' ', ' ', 0, 0, 0, 0, 'q', 't', ' ', ' '};
  appendBox(data, "ftyp", brands);

  std::vector<Exiv2::byte> camera(240, 0);
  camera[0] = 'N';
  camera[1] = 'I';
  camera[2] = 'K';
  camera[3] = 'O';
  for (size_t i = 175; i < 223; ++i)
    camera[i] = 'A';
  camera[223] = 1;

  std::vector<Exiv2::byte> userData;
  appendBox(userData, "TAGS", camera);
  appendBox(data, "udta", userData);

  auto image = Exiv2::ImageFactory::open(data.data(), data.size());
  if (!image)
    return 1;
  image->readMetadata();
  return 0;
}
