// Drives the public decoding API on a synthetically built, fully valid HEIF file:
// a planar 'unci' (ISO/IEC 23001-17 uncompressed) image with R, G, B and alpha
// components, each declared at 16 bits. The image is decoded to a YCbCr 4:2:0
// target while the alpha channel is composited onto a solid background (the same
// request heif-dec issues when the output format cannot store alpha).
//
// The file bytes are the only input; everything else is the documented public API.

#include "libheif/heif.h"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

constexpr uint32_t WIDTH = 128;
constexpr uint32_t HEIGHT = 128;

void append(std::vector<uint8_t>& dst, const std::vector<uint8_t>& src)
{
  dst.insert(dst.end(), src.begin(), src.end());
}

void put_u16_be(std::vector<uint8_t>& v, uint16_t x)
{
  v.push_back(static_cast<uint8_t>(x >> 8));
  v.push_back(static_cast<uint8_t>(x));
}

void put_u32_be(std::vector<uint8_t>& v, uint32_t x)
{
  v.push_back(static_cast<uint8_t>(x >> 24));
  v.push_back(static_cast<uint8_t>(x >> 16));
  v.push_back(static_cast<uint8_t>(x >> 8));
  v.push_back(static_cast<uint8_t>(x));
}

void append_fourcc(std::vector<uint8_t>& v, const char* fourcc)
{
  for (int i = 0; i < 4; i++) {
    v.push_back(static_cast<uint8_t>(fourcc[i]));
  }
}

std::vector<uint8_t> make_box(const char* fourcc, const std::vector<uint8_t>& payload,
                              bool full = false, uint8_t version = 0)
{
  std::vector<uint8_t> box;
  put_u32_be(box, static_cast<uint32_t>(8 + payload.size() + (full ? 4 : 0)));
  append_fourcc(box, fourcc);
  if (full) {
    box.push_back(version);
    box.push_back(0);
    box.push_back(0);
    box.push_back(0);
  }
  append(box, payload);
  return box;
}

// Minimal HEIF with a single 'unci' item: component (planar) interleave, 4:4:4,
// R = G = B = Alpha = 16 bit, uncompressed.
std::vector<uint8_t> build_heif_unci_rgba16()
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

  // iinf: item 1 = 'unci'.
  std::vector<uint8_t> infe_payload;
  put_u16_be(infe_payload, 1);
  put_u16_be(infe_payload, 0);
  append_fourcc(infe_payload, "unci");
  infe_payload.push_back(0);
  auto infe = make_box("infe", infe_payload, /*full=*/true, /*version=*/2);

  std::vector<uint8_t> iinf_payload;
  put_u16_be(iinf_payload, 1);
  append(iinf_payload, infe);
  auto iinf = make_box("iinf", iinf_payload, /*full=*/true);

  // ispe
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

  // uncC (v0): component (planar) interleave, 4:4:4, all four components 16 bit.
  std::vector<uint8_t> uncC_payload;
  put_u32_be(uncC_payload, 0); // profile
  put_u32_be(uncC_payload, 4); // component_count
  for (uint16_t idx = 0; idx < 4; idx++) {
    put_u16_be(uncC_payload, idx); // component_index
    uncC_payload.push_back(15);    // component_bit_depth_minus_one (16 bit)
    uncC_payload.push_back(0);     // component_format (unsigned)
    uncC_payload.push_back(0);     // component_align_size
  }
  uncC_payload.push_back(0); // sampling_type = 4:4:4
  uncC_payload.push_back(0); // interleave_type = component (planar)
  uncC_payload.push_back(0); // block_size
  uncC_payload.push_back(0); // flags (big-endian components)
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
  put_u32_be(ipma_payload, 1); // entry_count
  put_u16_be(ipma_payload, 1); // item_ID 1
  ipma_payload.push_back(3);   // association_count
  ipma_payload.push_back(0x80 | 1); // essential, ispe
  ipma_payload.push_back(0x80 | 2); // essential, cmpd
  ipma_payload.push_back(0x80 | 3); // essential, uncC
  auto ipma = make_box("ipma", ipma_payload, /*full=*/true);

  std::vector<uint8_t> iprp_payload;
  append(iprp_payload, ipco);
  append(iprp_payload, ipma);
  auto iprp = make_box("iprp", iprp_payload);

  // --- Image data (planar): R, G, B, Alpha planes, 2 bytes/sample big-endian.
  std::vector<uint8_t> tile_data;
  tile_data.reserve(WIDTH * HEIGHT * 2 * 4);

  for (uint32_t y = 0; y < HEIGHT; y++) // R
    for (uint32_t x = 0; x < WIDTH; x++)
      put_u16_be(tile_data, static_cast<uint16_t>(0x1000 + x + WIDTH * y));

  for (uint32_t y = 0; y < HEIGHT; y++) // G
    for (uint32_t x = 0; x < WIDTH; x++)
      put_u16_be(tile_data, static_cast<uint16_t>(0x2000 + x + WIDTH * y));

  for (uint32_t y = 0; y < HEIGHT; y++) // B
    for (uint32_t x = 0; x < WIDTH; x++)
      put_u16_be(tile_data, static_cast<uint16_t>(0x3000 + x + WIDTH * y));

  for (uint32_t y = 0; y < HEIGHT; y++) // Alpha (semi-transparent)
    for (uint32_t x = 0; x < WIDTH; x++)
      put_u16_be(tile_data, 0x8000);

  auto idat = make_box("idat", tile_data);

  // iloc (version 1): item 1 stored in idat (construction_method=1).
  std::vector<uint8_t> iloc_payload;
  put_u16_be(iloc_payload, (4 << 12) | (4 << 8) | (0 << 4) | 0); // offset_size=4, length_size=4
  put_u16_be(iloc_payload, 1); // item_count
  put_u16_be(iloc_payload, 1); // item_ID
  put_u16_be(iloc_payload, 0x0001); // construction_method=1 (idat)
  put_u16_be(iloc_payload, 0); // data_reference_index
  put_u16_be(iloc_payload, 1); // extent_count
  put_u32_be(iloc_payload, 0); // extent_offset (within idat)
  put_u32_be(iloc_payload, static_cast<uint32_t>(tile_data.size())); // extent_length
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

} // namespace

int main()
{
  std::vector<uint8_t> file = build_heif_unci_rgba16();

  heif_context* ctx = heif_context_alloc();
  if (ctx == nullptr) {
    fprintf(stderr, "heif_context_alloc failed\n");
    return 2;
  }

  heif_error err = heif_context_read_from_memory_without_copy(ctx, file.data(), file.size(), nullptr);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "read error: %s\n", err.message);
    return 2;
  }

  heif_image_handle* handle = nullptr;
  err = heif_context_get_primary_image_handle(ctx, &handle);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "handle error: %s\n", err.message);
    return 2;
  }

  if (!heif_image_handle_has_alpha_channel(handle)) {
    fprintf(stderr, "expected an alpha channel\n");
    return 2;
  }

  heif_decoding_options* options = heif_decoding_options_alloc();

  heif_color_conversion_options_ext* ext = heif_color_conversion_options_ext_alloc();
  ext->alpha_composition_mode = heif_alpha_composition_mode_solid_color;
  ext->background_red = 0xFFFF;
  ext->background_green = 0xFFFF;
  ext->background_blue = 0xFFFF;
  options->color_conversion_options_ext = ext;

  // Composite the alpha onto a solid background and convert to a YCbCr target
  // (the request heif-dec issues when writing an output format without alpha).
  heif_image* img = nullptr;
  err = heif_decode_image(handle, &img, heif_colorspace_YCbCr, heif_chroma_420, options);
  if (err.code != heif_error_Ok) {
    fprintf(stderr, "decode error (%d/%d): %s\n", err.code, err.subcode, err.message);
    return 1;
  }

  printf("decoded OK: %d x %d, chroma %d, has_alpha %d\n",
         heif_image_get_width(img, heif_channel_Y),
         heif_image_get_height(img, heif_channel_Y),
         heif_image_get_chroma_format(img),
         heif_image_has_channel(img, heif_channel_Alpha));

  heif_image_release(img);
  heif_color_conversion_options_ext_free(ext);
  heif_decoding_options_free(options);
  heif_image_handle_release(handle);
  heif_context_free(ctx);
  return 0;
}
