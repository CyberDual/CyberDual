// Decode a 10-bit YCbCr image with an alpha plane into a YCbCr target while
// compositing the alpha onto a solid background. This is the public path
// heif-dec takes when the output format cannot store alpha.

#include <libheif/heif.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

void append(std::vector<uint8_t>& out, const std::vector<uint8_t>& v)
{
  out.insert(out.end(), v.begin(), v.end());
}

void put_u16(std::vector<uint8_t>& out, uint16_t v)
{
  out.push_back(static_cast<uint8_t>(v >> 8));
  out.push_back(static_cast<uint8_t>(v));
}

void put_u32(std::vector<uint8_t>& out, uint32_t v)
{
  out.push_back(static_cast<uint8_t>(v >> 24));
  out.push_back(static_cast<uint8_t>(v >> 16));
  out.push_back(static_cast<uint8_t>(v >> 8));
  out.push_back(static_cast<uint8_t>(v));
}

void fourcc(std::vector<uint8_t>& out, const char t[4])
{
  out.insert(out.end(), t, t + 4);
}

void cstr(std::vector<uint8_t>& out, const char* s)
{
  out.insert(out.end(), s, s + std::strlen(s) + 1);
}

std::vector<uint8_t> box(const char t[4], const std::vector<uint8_t>& payload,
                         bool full = false, uint8_t version = 0)
{
  std::vector<uint8_t> body;
  if (full) {
    body.push_back(version);
    body.push_back(0);
    body.push_back(0);
    body.push_back(0);
  }
  append(body, payload);
  std::vector<uint8_t> out;
  put_u32(out, static_cast<uint32_t>(body.size() + 8));
  fourcc(out, t);
  append(out, body);
  return out;
}

std::vector<uint8_t> uncC_for(const uint8_t* depths, uint32_t n, uint8_t sampling)
{
  std::vector<uint8_t> p;
  put_u32(p, 0); // profile
  put_u32(p, n);
  for (uint32_t i = 0; i < n; i++) {
    put_u16(p, static_cast<uint16_t>(i));
    p.push_back(static_cast<uint8_t>(depths[i] - 1));
    p.push_back(0); // unsigned
    p.push_back(0); // align
  }
  p.push_back(sampling); // 4:4:4
  p.push_back(0);        // component interleave
  p.push_back(0);        // block size
  p.push_back(0);        // big-endian components
  put_u32(p, 0);
  put_u32(p, 0);
  put_u32(p, 0);
  put_u32(p, 0);
  put_u32(p, 0);
  return box("uncC", p, true);
}

void put_plane_u16(std::vector<uint8_t>& tile, uint32_t n, uint16_t value)
{
  for (uint32_t i = 0; i < n; i++) {
    put_u16(tile, value);
  }
}

// 10-bit 4:4:4 YCbCr primary plus a 10-bit monochrome alpha auxiliary.
std::vector<uint8_t> build_file()
{
  constexpr uint32_t W = 128;
  constexpr uint32_t H = 128;
  constexpr uint32_t N = W * H;

  std::vector<uint8_t> ftyp_p;
  fourcc(ftyp_p, "mif1");
  put_u32(ftyp_p, 0);
  fourcc(ftyp_p, "mif1");
  fourcc(ftyp_p, "heic");
  auto ftyp = box("ftyp", ftyp_p);

  std::vector<uint8_t> hdlr_p;
  put_u32(hdlr_p, 0);
  fourcc(hdlr_p, "pict");
  put_u32(hdlr_p, 0);
  put_u32(hdlr_p, 0);
  put_u32(hdlr_p, 0);
  hdlr_p.push_back(0);
  auto hdlr = box("hdlr", hdlr_p, true);

  std::vector<uint8_t> pitm_p;
  put_u16(pitm_p, 1);
  auto pitm = box("pitm", pitm_p, true);

  std::vector<uint8_t> iinf_p;
  put_u16(iinf_p, 2);
  for (uint16_t id = 1; id <= 2; id++) {
    std::vector<uint8_t> infe_p;
    put_u16(infe_p, id);
    put_u16(infe_p, 0);
    fourcc(infe_p, "unci");
    infe_p.push_back(0);
    append(iinf_p, box("infe", infe_p, true, 2));
  }
  auto iinf = box("iinf", iinf_p, true);

  std::vector<uint8_t> ispe_p;
  put_u32(ispe_p, W);
  put_u32(ispe_p, H);
  auto ispe = box("ispe", ispe_p, true);

  std::vector<uint8_t> cmpd_y_p;
  put_u32(cmpd_y_p, 3);
  put_u16(cmpd_y_p, 1); // Y
  put_u16(cmpd_y_p, 2); // Cb
  put_u16(cmpd_y_p, 3); // Cr
  auto cmpd_y = box("cmpd", cmpd_y_p);

  const uint8_t y_depths[3] = {10, 10, 10};
  auto unc_y = uncC_for(y_depths, 3, 0);

  std::vector<uint8_t> colr_p;
  fourcc(colr_p, "nclx");
  put_u16(colr_p, 6);
  put_u16(colr_p, 6);
  put_u16(colr_p, 6); // BT.601
  colr_p.push_back(0x80); // full range
  auto colr = box("colr", colr_p);

  std::vector<uint8_t> cmpd_a_p;
  put_u32(cmpd_a_p, 1);
  put_u16(cmpd_a_p, 1); // Y, decoded as the alpha plane
  auto cmpd_a = box("cmpd", cmpd_a_p);

  const uint8_t a_depths[1] = {10};
  auto unc_a = uncC_for(a_depths, 1, 0);

  std::vector<uint8_t> aux_p;
  cstr(aux_p, "urn:mpeg:mpegB:cicp:systems:auxiliary:alpha");
  auto auxC = box("auxC", aux_p, true);

  // ipco order: 1 ispe, 2 cmpd Y, 3 uncC Y, 4 colr, 5 cmpd A, 6 uncC A, 7 auxC
  std::vector<uint8_t> ipco_p;
  append(ipco_p, ispe);
  append(ipco_p, cmpd_y);
  append(ipco_p, unc_y);
  append(ipco_p, colr);
  append(ipco_p, cmpd_a);
  append(ipco_p, unc_a);
  append(ipco_p, auxC);
  auto ipco = box("ipco", ipco_p);

  std::vector<uint8_t> ipma_p;
  put_u32(ipma_p, 2);
  put_u16(ipma_p, 1);
  ipma_p.push_back(4);
  ipma_p.push_back(0x80 | 1);
  ipma_p.push_back(0x80 | 2);
  ipma_p.push_back(0x80 | 3);
  ipma_p.push_back(4); // colr, not essential
  put_u16(ipma_p, 2);
  ipma_p.push_back(4);
  ipma_p.push_back(0x80 | 1);
  ipma_p.push_back(0x80 | 5);
  ipma_p.push_back(0x80 | 6);
  ipma_p.push_back(0x80 | 7);
  auto ipma = box("ipma", ipma_p, true);

  std::vector<uint8_t> iprp_p;
  append(iprp_p, ipco);
  append(iprp_p, ipma);
  auto iprp = box("iprp", iprp_p);

  std::vector<uint8_t> tile;
  tile.reserve(static_cast<size_t>(N) * 2 * 4);
  put_plane_u16(tile, N, 400);  // Y
  put_plane_u16(tile, N, 512);  // Cb
  put_plane_u16(tile, N, 512);  // Cr
  const uint32_t color_bytes = static_cast<uint32_t>(tile.size());
  put_plane_u16(tile, N, 512);  // alpha, half coverage
  auto idat = box("idat", tile);

  std::vector<uint8_t> iloc_p;
  put_u16(iloc_p, static_cast<uint16_t>((4 << 12) | (4 << 8)));
  put_u16(iloc_p, 2);
  put_u16(iloc_p, 1);
  put_u16(iloc_p, 0x0001);
  put_u16(iloc_p, 0);
  put_u16(iloc_p, 1);
  put_u32(iloc_p, 0);
  put_u32(iloc_p, color_bytes);
  put_u16(iloc_p, 2);
  put_u16(iloc_p, 0x0001);
  put_u16(iloc_p, 0);
  put_u16(iloc_p, 1);
  put_u32(iloc_p, color_bytes);
  put_u32(iloc_p, static_cast<uint32_t>(tile.size() - color_bytes));
  auto iloc = box("iloc", iloc_p, true, 1);

  std::vector<uint8_t> auxl_p;
  put_u16(auxl_p, 2); // from alpha item
  put_u16(auxl_p, 1); // one reference
  put_u16(auxl_p, 1); // to the color item
  auto auxl = box("auxl", auxl_p);
  auto iref = box("iref", auxl, true);

  std::vector<uint8_t> meta_p;
  append(meta_p, hdlr);
  append(meta_p, pitm);
  append(meta_p, iinf);
  append(meta_p, iprp);
  append(meta_p, iloc);
  append(meta_p, iref);
  append(meta_p, idat);
  auto meta = box("meta", meta_p, true);

  std::vector<uint8_t> file;
  append(file, ftyp);
  append(file, meta);
  return file;
}

} // namespace

int main()
{
  std::vector<uint8_t> file = build_file();

  heif_context* ctx = heif_context_alloc();
  heif_error err = heif_context_read_from_memory_without_copy(ctx, file.data(), file.size(), nullptr);
  if (err.code != heif_error_Ok) {
    std::fprintf(stderr, "read failed: %s\n", err.message);
    return 0;
  }

  heif_image_handle* handle = nullptr;
  err = heif_context_get_primary_image_handle(ctx, &handle);
  if (err.code != heif_error_Ok || handle == nullptr) {
    std::fprintf(stderr, "primary handle failed: %s\n", err.message);
    return 0;
  }

  if (!heif_image_handle_has_alpha_channel(handle)) {
    std::fprintf(stderr, "image has no alpha channel\n");
    return 0;
  }

  heif_decoding_options* options = heif_decoding_options_alloc();
  heif_color_conversion_options_ext* ext = heif_color_conversion_options_ext_alloc();
  ext->alpha_composition_mode = heif_alpha_composition_mode_solid_color;
  ext->background_red = 0xFFFF;
  ext->background_green = 0xFFFF;
  ext->background_blue = 0xFFFF;
  options->color_conversion_options_ext = ext;

  heif_image* img = nullptr;
  err = heif_decode_image(handle, &img, heif_colorspace_YCbCr, heif_chroma_444, options);
  if (err.code != heif_error_Ok) {
    std::fprintf(stderr, "decode failed (%d/%d): %s\n", err.code, err.subcode, err.message);
  } else {
    std::fprintf(stderr, "decode ok %dx%d bpp %d\n",
                 heif_image_get_width(img, heif_channel_Y),
                 heif_image_get_height(img, heif_channel_Y),
                 heif_image_get_bits_per_pixel_range(img, heif_channel_Y));
    heif_image_release(img);
  }

  heif_color_conversion_options_ext_free(ext);
  heif_decoding_options_free(options);
  heif_image_handle_release(handle);
  heif_context_free(ctx);
  return 0;
}
