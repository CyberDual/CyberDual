/*
 * Drives Exiv2's public API with an in-memory EPS file that carries an
 * Illustrator (AI7) thumbnail preview, then asks for the preview images.
 *
 * The thumbnail is a 7x1 8-bit image: 768 bytes of color table followed by
 * 8 bytes of literal (non-RLE) pixel data, hex-encoded as required by the
 * EPS %%BeginData: ... Hex Bytes convention.
 */
#include <exiv2/exiv2.hpp>

#include <string>

int main(void) {
  std::string eps;
  eps += "%!PS-Adobe-3.0 EPSF-3.0\n";
  eps += "%%Creator: probe\n";
  eps += "%%BoundingBox: 0 0 7 1\n";
  eps += "%AI7_Thumbnail: 7 1 8\n";
  eps += "%%BeginData: 1552 Hex Bytes\n";
  eps += "%";
  eps.append(768 * 2, '0');  /* color table: 256 x RGB, all zero */
  eps.append(8 * 2, '0');    /* 8 literal palette indices */
  eps += "\n%%EndData\n";
  eps += "%%EndComments\n";
  eps += "showpage\n";

  try {
    Exiv2::Image::UniquePtr image =
        Exiv2::ImageFactory::open(reinterpret_cast<const Exiv2::byte*>(eps.data()), eps.size());
    if (!image)
      return 1;
    image->readMetadata();

    Exiv2::PreviewManager manager(*image);
    const Exiv2::PreviewPropertiesList properties = manager.getPreviewProperties();
    for (const Exiv2::PreviewProperties& p : properties) {
      const Exiv2::PreviewImage preview = manager.getPreviewImage(p);
      if (preview.size() == 0)
        return 1;
    }
  } catch (const Exiv2::Error&) {
    /* Unreadable or unsupported metadata is a normal, handled outcome. */
    return 0;
  }
  return 0;
}
