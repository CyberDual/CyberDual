// In-contract uncv 4:2:0 sequence: 128x129 luma, component interleave.
// Decoded through heif_track_decode_next_image() to interleaved RGB.

#include <libheif/heif.h>
#include <libheif/heif_sequences.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

static void put16(std::vector<uint8_t>& v, uint16_t x)
{
  v.push_back(static_cast<uint8_t>(x >> 8));
  v.push_back(static_cast<uint8_t>(x));
}

static void put32(std::vector<uint8_t>& v, uint32_t x)
{
  v.push_back(static_cast<uint8_t>(x >> 24));
  v.push_back(static_cast<uint8_t>(x >> 16));
  v.push_back(static_cast<uint8_t>(x >> 8));
  v.push_back(static_cast<uint8_t>(x));
}

static void put_fourcc(std::vector<uint8_t>& v, const char* s)
{
  v.push_back(static_cast<uint8_t>(s[0]));
  v.push_back(static_cast<uint8_t>(s[1]));
  v.push_back(static_cast<uint8_t>(s[2]));
  v.push_back(static_cast<uint8_t>(s[3]));
}

static std::vector<uint8_t> box(const char* type, const std::vector<uint8_t>& payload)
{
  std::vector<uint8_t> b;
  put32(b, static_cast<uint32_t>(8 + payload.size()));
  put_fourcc(b, type);
  b.insert(b.end(), payload.begin(), payload.end());
  return b;
}

static std::vector<uint8_t> concat(std::initializer_list<std::vector<uint8_t>> parts)
{
  std::vector<uint8_t> out;
  for (const auto& p : parts) {
    out.insert(out.end(), p.begin(), p.end());
  }
  return out;
}

static std::vector<uint8_t> build_uncv_sequence(uint16_t width, uint16_t height)
{
  std::vector<uint8_t> ftyp_p;
  put_fourcc(ftyp_p, "msf1");
  put32(ftyp_p, 0);
  put_fourcc(ftyp_p, "msf1");
  put_fourcc(ftyp_p, "iso8");
  std::vector<uint8_t> ftyp = box("ftyp", ftyp_p);

  std::vector<uint8_t> mvhd_p;
  put32(mvhd_p, 0);
  put32(mvhd_p, 0);
  put32(mvhd_p, 0);
  put32(mvhd_p, 1000);
  put32(mvhd_p, 1);
  put32(mvhd_p, 0x00010000);
  put16(mvhd_p, 0x0100);
  put16(mvhd_p, 0);
  put32(mvhd_p, 0);
  put32(mvhd_p, 0);
  for (uint32_t m : {0x00010000u, 0u, 0u, 0u, 0x00010000u, 0u, 0u, 0u, 0x40000000u}) put32(mvhd_p, m);
  for (int i = 0; i < 6; i++) put32(mvhd_p, 0);
  put32(mvhd_p, 2);
  std::vector<uint8_t> mvhd = box("mvhd", mvhd_p);

  std::vector<uint8_t> tkhd_p;
  put32(tkhd_p, 0x00000007);
  put32(tkhd_p, 0);
  put32(tkhd_p, 0);
  put32(tkhd_p, 1);
  put32(tkhd_p, 0);
  put32(tkhd_p, 1);
  put32(tkhd_p, 0);
  put32(tkhd_p, 0);
  put16(tkhd_p, 0);
  put16(tkhd_p, 0);
  put16(tkhd_p, 0);
  put16(tkhd_p, 0);
  for (uint32_t m : {0x00010000u, 0u, 0u, 0u, 0x00010000u, 0u, 0u, 0u, 0x40000000u}) put32(tkhd_p, m);
  put32(tkhd_p, static_cast<uint32_t>(width) << 16);
  put32(tkhd_p, static_cast<uint32_t>(height) << 16);
  std::vector<uint8_t> tkhd = box("tkhd", tkhd_p);

  std::vector<uint8_t> mdhd_p;
  put32(mdhd_p, 0);
  put32(mdhd_p, 0);
  put32(mdhd_p, 0);
  put32(mdhd_p, 1000);
  put32(mdhd_p, 1);
  put16(mdhd_p, 0x55c4);
  put16(mdhd_p, 0);
  std::vector<uint8_t> mdhd = box("mdhd", mdhd_p);

  std::vector<uint8_t> hdlr_p;
  put32(hdlr_p, 0);
  put32(hdlr_p, 0);
  put_fourcc(hdlr_p, "pict");
  put32(hdlr_p, 0);
  put32(hdlr_p, 0);
  put32(hdlr_p, 0);
  hdlr_p.push_back(0);
  std::vector<uint8_t> hdlr = box("hdlr", hdlr_p);

  std::vector<uint8_t> vmhd_p;
  put32(vmhd_p, 0x00000001);
  put16(vmhd_p, 0);
  put16(vmhd_p, 0);
  put16(vmhd_p, 0);
  put16(vmhd_p, 0);
  std::vector<uint8_t> vmhd = box("vmhd", vmhd_p);

  std::vector<uint8_t> cmpd_p;
  put32(cmpd_p, 3);
  put16(cmpd_p, 1);
  put16(cmpd_p, 2);
  put16(cmpd_p, 3);
  std::vector<uint8_t> cmpd = box("cmpd", cmpd_p);

  std::vector<uint8_t> uncC_p;
  put32(uncC_p, 0);
  put32(uncC_p, 0);
  put32(uncC_p, 3);
  for (uint16_t idx = 0; idx < 3; idx++) {
    put16(uncC_p, idx);
    uncC_p.push_back(7);
    uncC_p.push_back(0);
    uncC_p.push_back(0);
  }
  uncC_p.push_back(2);
  uncC_p.push_back(0);
  uncC_p.push_back(0);
  uncC_p.push_back(0);
  put32(uncC_p, 0);
  put32(uncC_p, 0);
  put32(uncC_p, 0);
  put32(uncC_p, 0);
  put32(uncC_p, 0);
  std::vector<uint8_t> uncC = box("uncC", uncC_p);

  std::vector<uint8_t> vse;
  for (int i = 0; i < 6; i++) vse.push_back(0);
  put16(vse, 1);
  put16(vse, 0);
  put16(vse, 0);
  put32(vse, 0);
  put32(vse, 0);
  put32(vse, 0);
  put16(vse, width);
  put16(vse, height);
  put32(vse, 0x00480000);
  put32(vse, 0x00480000);
  put32(vse, 0);
  put16(vse, 1);
  for (int i = 0; i < 32; i++) vse.push_back(0);
  put16(vse, 0x0018);
  put16(vse, 0xFFFF);
  std::vector<uint8_t> uncv = box("uncv", concat({vse, cmpd, uncC}));

  std::vector<uint8_t> stsd_p;
  put32(stsd_p, 0);
  put32(stsd_p, 1);
  stsd_p.insert(stsd_p.end(), uncv.begin(), uncv.end());
  std::vector<uint8_t> stsd = box("stsd", stsd_p);

  std::vector<uint8_t> stts_p;
  put32(stts_p, 0);
  put32(stts_p, 1);
  put32(stts_p, 1);
  put32(stts_p, 1);
  std::vector<uint8_t> stts = box("stts", stts_p);

  std::vector<uint8_t> stsc_p;
  put32(stsc_p, 0);
  put32(stsc_p, 1);
  put32(stsc_p, 1);
  put32(stsc_p, 1);
  put32(stsc_p, 1);
  std::vector<uint8_t> stsc = box("stsc", stsc_p);

  const uint32_t sample_size =
      static_cast<uint32_t>(width) * height +
      2u * (width / 2) * (height / 2);

  std::vector<uint8_t> stsz_p;
  put32(stsz_p, 0);
  put32(stsz_p, sample_size);
  put32(stsz_p, 1);
  std::vector<uint8_t> stsz = box("stsz", stsz_p);

  auto assemble = [&](uint32_t chunk_offset) {
    std::vector<uint8_t> stco_p;
    put32(stco_p, 0);
    put32(stco_p, 1);
    put32(stco_p, chunk_offset);
    std::vector<uint8_t> stco = box("stco", stco_p);

    std::vector<uint8_t> stbl = box("stbl", concat({stsd, stts, stsc, stsz, stco}));
    std::vector<uint8_t> minf = box("minf", concat({vmhd, stbl}));
    std::vector<uint8_t> mdia = box("mdia", concat({mdhd, hdlr, minf}));
    std::vector<uint8_t> trak = box("trak", concat({tkhd, mdia}));
    return box("moov", concat({mvhd, trak}));
  };

  std::vector<uint8_t> moov0 = assemble(0);
  uint32_t mdat_payload_off = static_cast<uint32_t>(ftyp.size() + moov0.size() + 8);
  std::vector<uint8_t> moov = assemble(mdat_payload_off);

  std::vector<uint8_t> mdat_payload;
  mdat_payload.reserve(sample_size);
  for (uint32_t y = 0; y < height; y++)
    for (uint32_t x = 0; x < width; x++)
      mdat_payload.push_back(static_cast<uint8_t>((x + y) & 0xFF));
  for (int plane = 0; plane < 2; plane++)
    for (uint32_t y = 0; y < height / 2u; y++)
      for (uint32_t x = 0; x < width / 2u; x++)
        mdat_payload.push_back(0x80);
  std::vector<uint8_t> mdat = box("mdat", mdat_payload);

  return concat({ftyp, moov, mdat});
}

int main(int argc, char** argv)
{
  uint16_t width = 128;
  uint16_t height = 129;
  if (argc >= 3) {
    width = static_cast<uint16_t>(std::atoi(argv[1]));
    height = static_cast<uint16_t>(std::atoi(argv[2]));
  }

  std::vector<uint8_t> file = build_uncv_sequence(width, height);

  heif_context* ctx = heif_context_alloc();
  if (!ctx) {
    std::fprintf(stderr, "context alloc failed\n");
    return 2;
  }

  heif_error err = heif_context_read_from_memory(ctx, file.data(), file.size(), nullptr);
  if (err.code != heif_error_Ok) {
    std::fprintf(stderr, "read failed: %s\n", err.message ? err.message : "");
    heif_context_free(ctx);
    return 2;
  }

  heif_track* track = heif_context_get_track(ctx, 0);
  if (!track) {
    std::fprintf(stderr, "no track\n");
    heif_context_free(ctx);
    return 2;
  }

  heif_image* img = nullptr;
  heif_error derr = heif_track_decode_next_image(track, &img,
                                                 heif_colorspace_RGB,
                                                 heif_chroma_interleaved_RGB,
                                                 nullptr);
  if (derr.code != heif_error_Ok || img == nullptr) {
    std::fprintf(stderr, "decode failed: %s\n", derr.message ? derr.message : "");
    heif_track_release(track);
    heif_context_free(ctx);
    return 2;
  }

  int w = heif_image_get_width(img, heif_channel_interleaved);
  int h = heif_image_get_height(img, heif_channel_interleaved);
  std::printf("decoded %dx%d\n", w, h);

  heif_image_release(img);
  heif_track_release(track);
  heif_context_free(ctx);
  return 0;
}
