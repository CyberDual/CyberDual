#include <libheif/heif.h>

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace {

void put_u32_be(std::vector<uint8_t>& out, uint32_t v)
{
  out.push_back(static_cast<uint8_t>(v >> 24));
  out.push_back(static_cast<uint8_t>(v >> 16));
  out.push_back(static_cast<uint8_t>(v >> 8));
  out.push_back(static_cast<uint8_t>(v));
}

void put_u16_be(std::vector<uint8_t>& out, uint16_t v)
{
  out.push_back(static_cast<uint8_t>(v >> 8));
  out.push_back(static_cast<uint8_t>(v));
}

void append_fourcc(std::vector<uint8_t>& out, const char fourcc[4])
{
  for (int i = 0; i < 4; i++) {
    out.push_back(static_cast<uint8_t>(fourcc[i]));
  }
}

void append(std::vector<uint8_t>& out, const std::vector<uint8_t>& v)
{
  out.insert(out.end(), v.begin(), v.end());
}

std::vector<uint8_t> make_box(const char fourcc[4], const std::vector<uint8_t>& payload, bool is_full_box, uint8_t version = 0)
{
  const size_t header_size = is_full_box ? 12 : 8;
  std::vector<uint8_t> box;
  put_u32_be(box, static_cast<uint32_t>(header_size + payload.size()));
  append_fourcc(box, fourcc);
  if (is_full_box) {
    box.push_back(version);
    box.push_back(0);
    box.push_back(0);
    box.push_back(0);
  }
  append(box, payload);
  return box;
}

// Minimal HEIF: an 'iovl' places one 'mski' input twice. The second offset
// shifts the image past the right edge of the canvas, which ISO/IEC 23008-12
// allows (the overhang is clipped).
std::vector<uint8_t> build_repeated_overlay()
{
  const uint16_t tile = 16;
  const uint16_t canvas = 16;
  const uint16_t second_x = 8;

  std::vector<uint8_t> ftyp_payload;
  append_fourcc(ftyp_payload, "heic");
  put_u32_be(ftyp_payload, 0);
  append_fourcc(ftyp_payload, "mif1");
  append_fourcc(ftyp_payload, "heic");
  auto ftyp = make_box("ftyp", ftyp_payload, false);

  std::vector<uint8_t> hdlr_payload;
  put_u32_be(hdlr_payload, 0);
  append_fourcc(hdlr_payload, "pict");
  put_u32_be(hdlr_payload, 0);
  put_u32_be(hdlr_payload, 0);
  put_u32_be(hdlr_payload, 0);
  hdlr_payload.push_back(0);
  auto hdlr = make_box("hdlr", hdlr_payload, true);

  std::vector<uint8_t> pitm_payload;
  put_u16_be(pitm_payload, 2);
  auto pitm = make_box("pitm", pitm_payload, true);

  std::vector<uint8_t> iinf_payload;
  put_u16_be(iinf_payload, 2);
  for (int id = 1; id <= 2; id++) {
    std::vector<uint8_t> infe_payload;
    put_u16_be(infe_payload, static_cast<uint16_t>(id));
    put_u16_be(infe_payload, 0);
    append_fourcc(infe_payload, id == 1 ? "mski" : "iovl");
    infe_payload.push_back(0);
    append(iinf_payload, make_box("infe", infe_payload, true, 2));
  }
  auto iinf = make_box("iinf", iinf_payload, true);

  std::vector<uint8_t> ispe_payload;
  put_u32_be(ispe_payload, canvas);
  put_u32_be(ispe_payload, canvas);
  auto ispe = make_box("ispe", ispe_payload, true);

  std::vector<uint8_t> mskC_payload;
  mskC_payload.push_back(8);
  auto mskC = make_box("mskC", mskC_payload, true);

  std::vector<uint8_t> ipco_payload;
  append(ipco_payload, ispe);
  append(ipco_payload, mskC);
  auto ipco = make_box("ipco", ipco_payload, false);

  std::vector<uint8_t> ipma_payload;
  put_u32_be(ipma_payload, 2);
  put_u16_be(ipma_payload, 1);
  ipma_payload.push_back(2);
  ipma_payload.push_back(0x80 | 1);
  ipma_payload.push_back(0x80 | 2);
  put_u16_be(ipma_payload, 2);
  ipma_payload.push_back(1);
  ipma_payload.push_back(0x80 | 1);
  auto ipma = make_box("ipma", ipma_payload, true);

  std::vector<uint8_t> iprp_payload;
  append(iprp_payload, ipco);
  append(iprp_payload, ipma);
  auto iprp = make_box("iprp", iprp_payload, false);

  std::vector<uint8_t> mask(static_cast<size_t>(tile) * tile, 0x7F);

  std::vector<uint8_t> iovl_spec;
  iovl_spec.push_back(0);
  iovl_spec.push_back(0);
  for (int i = 0; i < 4; i++) {
    put_u16_be(iovl_spec, 0);
  }
  put_u16_be(iovl_spec, canvas);
  put_u16_be(iovl_spec, canvas);
  put_u16_be(iovl_spec, 0);
  put_u16_be(iovl_spec, 0);
  put_u16_be(iovl_spec, second_x);
  put_u16_be(iovl_spec, 0);

  std::vector<uint8_t> idat_payload;
  uint32_t mask_off = 0;
  append(idat_payload, mask);
  uint32_t iovl_off = static_cast<uint32_t>(idat_payload.size());
  append(idat_payload, iovl_spec);
  auto idat = make_box("idat", idat_payload, false);

  std::vector<uint8_t> iloc_payload;
  iloc_payload.push_back((4 << 4) | 4);
  iloc_payload.push_back(0);
  put_u16_be(iloc_payload, 2);
  put_u16_be(iloc_payload, 1);
  put_u16_be(iloc_payload, 0x0001);
  put_u16_be(iloc_payload, 0);
  put_u16_be(iloc_payload, 1);
  put_u32_be(iloc_payload, mask_off);
  put_u32_be(iloc_payload, static_cast<uint32_t>(mask.size()));
  put_u16_be(iloc_payload, 2);
  put_u16_be(iloc_payload, 0x0001);
  put_u16_be(iloc_payload, 0);
  put_u16_be(iloc_payload, 1);
  put_u32_be(iloc_payload, iovl_off);
  put_u32_be(iloc_payload, static_cast<uint32_t>(iovl_spec.size()));
  auto iloc = make_box("iloc", iloc_payload, true, 1);

  std::vector<uint8_t> dimg_payload;
  put_u16_be(dimg_payload, 2);
  put_u16_be(dimg_payload, 2);
  put_u16_be(dimg_payload, 1);
  put_u16_be(dimg_payload, 1);
  auto iref = make_box("iref", make_box("dimg", dimg_payload, false), true);

  std::vector<uint8_t> meta_payload;
  append(meta_payload, hdlr);
  append(meta_payload, pitm);
  append(meta_payload, iinf);
  append(meta_payload, iprp);
  append(meta_payload, iloc);
  append(meta_payload, iref);
  append(meta_payload, idat);
  auto meta = make_box("meta", meta_payload, true);

  std::vector<uint8_t> file;
  append(file, ftyp);
  append(file, meta);
  return file;
}

}  // namespace

int main()
{
  const char* path = "/tmp/iref_repeat_clip.heif";
  std::vector<uint8_t> file = build_repeated_overlay();
  {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
      std::perror(path);
      return 1;
    }
    out.write(reinterpret_cast<const char*>(file.data()), static_cast<std::streamsize>(file.size()));
  }

  heif_context* ctx = heif_context_alloc();
  if (!ctx) {
    return 1;
  }

  heif_error err = heif_context_read_from_file(ctx, path, nullptr);
  if (err.code != heif_error_Ok) {
    std::fprintf(stderr, "read failed: %s\n", err.message ? err.message : "");
    heif_context_free(ctx);
    return 0;
  }

  heif_image_handle* handle = nullptr;
  err = heif_context_get_primary_image_handle(ctx, &handle);
  if (err.code != heif_error_Ok) {
    std::fprintf(stderr, "primary failed: %s\n", err.message ? err.message : "");
    heif_context_free(ctx);
    return 0;
  }

  heif_image* img = nullptr;
  err = heif_decode_image(handle, &img, heif_colorspace_undefined, heif_chroma_undefined, nullptr);
  if (img) {
    heif_image_release(img);
  }
  heif_image_handle_release(handle);
  heif_context_free(ctx);

  if (err.code != heif_error_Ok) {
    std::fprintf(stderr, "decode failed: %s\n", err.message ? err.message : "");
  }
  return 0;
}
