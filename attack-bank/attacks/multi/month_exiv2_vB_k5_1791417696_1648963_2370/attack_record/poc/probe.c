#include <exiv2/exiv2.hpp>

#include <cstdint>
#include <cstring>
#include <vector>

static void append32(std::vector<Exiv2::byte>& out, uint32_t value) {
  out.push_back(static_cast<Exiv2::byte>(value >> 24));
  out.push_back(static_cast<Exiv2::byte>(value >> 16));
  out.push_back(static_cast<Exiv2::byte>(value >> 8));
  out.push_back(static_cast<Exiv2::byte>(value));
}

static void appendTag(std::vector<Exiv2::byte>& out, const char tag[5]) {
  out.insert(out.end(), tag, tag + 4);
}

static void appendAtom(std::vector<Exiv2::byte>& out, const char tag[5],
                       const std::vector<Exiv2::byte>& payload) {
  append32(out, static_cast<uint32_t>(payload.size() + 8));
  appendTag(out, tag);
  out.insert(out.end(), payload.begin(), payload.end());
}

int main(void) {
  std::vector<Exiv2::byte> file;
  std::vector<Exiv2::byte> ftyp;
  appendTag(ftyp, "qt  ");
  append32(ftyp, 0);
  appendAtom(file, "ftyp", ftyp);

  std::vector<Exiv2::byte> media;
  std::vector<Exiv2::byte> handler(20, 0);
  std::memcpy(handler.data() + 8, "vide", 4);
  appendAtom(media, "hdlr", handler);

  std::vector<Exiv2::byte> description;
  append32(description, 0);
  append32(description, 1);
  append32(description, 86);
  std::vector<Exiv2::byte> entry(82, 0);
  std::memcpy(entry.data(), "avc1", 4);
  std::memcpy(entry.data() + 44, "ABCDEFGHIJKLMNOPQRSTUVWXYZ123456", 32);
  description.insert(description.end(), entry.begin(), entry.end());
  std::vector<Exiv2::byte> sample_table;
  appendAtom(sample_table, "stsd", description);
  std::vector<Exiv2::byte> media_info;
  appendAtom(media_info, "stbl", sample_table);
  appendAtom(media, "minf", media_info);
  std::vector<Exiv2::byte> track;
  appendAtom(track, "mdia", media);
  std::vector<Exiv2::byte> movie;
  appendAtom(movie, "trak", track);
  appendAtom(file, "moov", movie);

  auto image = Exiv2::ImageFactory::open(file.data(), file.size());
  if (!image)
    return 1;
  image->readMetadata();
  return 0;
}
