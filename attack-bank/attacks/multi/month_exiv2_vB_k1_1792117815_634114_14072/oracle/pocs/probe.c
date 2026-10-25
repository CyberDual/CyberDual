#include <exiv2/exiv2.hpp>

#include <cstdint>
#include <cstring>
#include <vector>

static void append32(std::vector<uint8_t>& out, uint32_t v) {
  out.push_back(static_cast<uint8_t>(v >> 24));
  out.push_back(static_cast<uint8_t>(v >> 16));
  out.push_back(static_cast<uint8_t>(v >> 8));
  out.push_back(static_cast<uint8_t>(v));
}

static void append(std::vector<uint8_t>& out, const char* s, size_t n) {
  out.insert(out.end(), s, s + n);
}

/* QuickTime file the public image API accepts: ftyp brand "qt  ", then a
   moov/udta/NCTG atom holding one Nikon string tag of length 200 — the
   largest payload the NCTG decoder's own diagnostic treats as valid. */
int main(void) {
  const uint16_t data_length = 200;
  std::vector<uint8_t> nctg;
  append32(nctg, 1);             /* tag id */
  nctg.push_back(0x00);
  nctg.push_back(0x02);          /* data type: string */
  nctg.push_back(static_cast<uint8_t>(data_length >> 8));
  nctg.push_back(static_cast<uint8_t>(data_length));
  nctg.insert(nctg.end(), data_length, static_cast<uint8_t>('A'));

  std::vector<uint8_t> udta_payload;
  append32(udta_payload, static_cast<uint32_t>(8 + nctg.size()));
  append(udta_payload, "NCTG", 4);
  udta_payload.insert(udta_payload.end(), nctg.begin(), nctg.end());

  std::vector<uint8_t> moov_payload;
  append32(moov_payload, static_cast<uint32_t>(8 + udta_payload.size()));
  append(moov_payload, "udta", 4);
  moov_payload.insert(moov_payload.end(), udta_payload.begin(), udta_payload.end());

  std::vector<uint8_t> file;
  append32(file, 16);
  append(file, "ftyp", 4);
  append(file, "qt  ", 4);
  append32(file, 0);
  append32(file, static_cast<uint32_t>(8 + moov_payload.size()));
  append(file, "moov", 4);
  file.insert(file.end(), moov_payload.begin(), moov_payload.end());

  try {
    Exiv2::Image::UniquePtr img = Exiv2::ImageFactory::open(file.data(), file.size());
    if (img.get() != nullptr)
      img->readMetadata();
  } catch (const Exiv2::Error&) {
    return 0;
  } catch (...) {
    return 0;
  }
  return 0;
}
