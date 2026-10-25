#include "libheif/heif.h"
#include "libheif/heif_security.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <utility>
#include <unistd.h>

using Bytes = std::vector<uint8_t>;
static void u16(Bytes& v, uint16_t x) { v.push_back(x >> 8); v.push_back(x); }
static void u32(Bytes& v, uint32_t x) { v.push_back(x >> 24); v.push_back(x >> 16); v.push_back(x >> 8); v.push_back(x); }
static void fourcc(Bytes& v, const char* s) { for (int i = 0; i < 4; ++i) v.push_back(s[i]); }
static void append(Bytes& v, const Bytes& x) { v.insert(v.end(), x.begin(), x.end()); }
static Bytes box(const char* name, Bytes data, bool full = false, uint8_t version = 0) {
  Bytes b; u32(b, static_cast<uint32_t>(data.size() + 8 + (full ? 4 : 0)));
  fourcc(b, name);
  if (full) { b.push_back(version); b.insert(b.end(), 3, 0); }
  append(b, data); return b;
}
int main() {
  Bytes ftyp_p; fourcc(ftyp_p, "heic"); u32(ftyp_p, 0); fourcc(ftyp_p, "mif1"); fourcc(ftyp_p, "heic");
  Bytes file = box("ftyp", ftyp_p);
  Bytes hdlr_p; u32(hdlr_p, 0); fourcc(hdlr_p, "pict"); u32(hdlr_p, 0); u32(hdlr_p, 0); u32(hdlr_p, 0); hdlr_p.push_back(0);
  Bytes pitm_p; u16(pitm_p, 1);
  Bytes iinf_p; u16(iinf_p, 2);
  for (auto [id, type] : {std::pair<uint16_t, const char*>{1, "iovl"}, {2, "mski"}}) {
    Bytes infe_p; u16(infe_p, id); u16(infe_p, 0); fourcc(infe_p, type); infe_p.push_back(0);
    append(iinf_p, box("infe", infe_p, true, 2));
  }
  Bytes ispe_p; u32(ispe_p, 8); u32(ispe_p, 8);
  Bytes mskc_p{8};
  Bytes ipco_p; append(ipco_p, box("ispe", ispe_p, true)); append(ipco_p, box("mskC", mskc_p, true));
  Bytes ipma_p; u32(ipma_p, 2); u16(ipma_p, 1); ipma_p.push_back(1); ipma_p.push_back(0x81);
  u16(ipma_p, 2); ipma_p.push_back(2); ipma_p.push_back(0x81); ipma_p.push_back(0x82);
  Bytes iprp_p; append(iprp_p, box("ipco", ipco_p)); append(iprp_p, box("ipma", ipma_p, true));
  Bytes overlay{0, 0}; overlay.insert(overlay.end(), 8, 0); u16(overlay, 8); u16(overlay, 8);
  for (int i = 0; i < 65535; ++i) { u16(overlay, 0); u16(overlay, 0); }
  Bytes idat_p = overlay; idat_p.insert(idat_p.end(), 64, 0x7f);
  Bytes iloc_p{0x44, 0x00}; u16(iloc_p, 2);
  for (int id = 1; id <= 2; ++id) {
    u16(iloc_p, id); u16(iloc_p, 1); u16(iloc_p, 0); u16(iloc_p, 1);
    u32(iloc_p, id == 1 ? 0 : static_cast<uint32_t>(overlay.size()));
    u32(iloc_p, id == 1 ? static_cast<uint32_t>(overlay.size()) : 64);
  }
  Bytes dimg_p; u16(dimg_p, 1); u16(dimg_p, 65535);
  for (int i = 0; i < 65535; ++i) u16(dimg_p, 2);
  Bytes meta_p;
  append(meta_p, box("hdlr", hdlr_p, true));
  append(meta_p, box("pitm", pitm_p, true));
  append(meta_p, box("iinf", iinf_p, true));
  append(meta_p, box("iprp", iprp_p));
  append(meta_p, box("iloc", iloc_p, true, 1));
  append(meta_p, box("iref", box("dimg", dimg_p), true));
  append(meta_p, box("idat", idat_p));
  append(file, box("meta", meta_p, true));
  char path[] = "/tmp/heif-iref-XXXXXX";
  int fd = mkstemp(path);
  if (fd < 0) return 2;
  FILE* out = fdopen(fd, "wb");
  if (!out || fwrite(file.data(), 1, file.size(), out) != file.size()) return 2;
  fclose(out);
  heif_context* ctx = heif_context_alloc();
  heif_security_limits limits = *heif_context_get_security_limits(ctx);
  limits.max_items = 0;
  heif_context_set_security_limits(ctx, &limits);
  heif_error err = heif_context_read_from_file(ctx, path, nullptr);
  printf("read: %d %s\n", err.code, err.message ? err.message : "");
  heif_context_free(ctx);
  unlink(path);
  return err.code == heif_error_Ok ? 0 : 1;
}
