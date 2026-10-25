// SPDX-License-Identifier: 0BSD
//
// Probe for the liblzma .lzma (LZMA_Alone) decoder reinitialization path.
//
// Uses only the public <lzma.h> API:
//
//   - lzma_alone_decoder() is called three times on the same lzma_stream.
//   - The first pass decodes a valid .lzma stream with a 4 KiB dictionary.
//   - The second pass presents a header with a 64 MiB dictionary. A custom
//     lzma_allocator rejects the dictionary allocation, so lzma_code()
//     returns LZMA_MEM_ERROR. Reusing a stream after LZMA_MEM_ERROR is
//     documented, supported behavior.
//   - The third pass decodes the original 4 KiB-dictionary stream again.
//
// On a healthy liblzma the third pass succeeds (LZMA_STREAM_END) and the
// program exits with status 0.

#include <lzma.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// Allocations of this size or bigger are refused. Only the 64 MiB
// dictionary allocation in the second pass is this big; every other
// allocation in the decoder is a few KiB.
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


static lzma_allocator my_allocator = {
	.alloc = &my_alloc,
	.free = &my_free,
	.opaque = NULL,
};


// A valid LZMA_Alone file: lc=3 lp=0 pb=2, dictionary size 0x00001000
// (4 KiB), uncompressed size unknown (all 0xFF), LZMA1 data with an
// end-of-payload marker.
static const uint8_t small_dict_file[] = {
	0x5d, 0x00, 0x10, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0x00, 0x24, 0x19, 0x49, 0x98, 0x6f, 0x05,
	0x15, 0x27, 0x27, 0x0d, 0x76, 0x78, 0xd0, 0x2a, 0x68, 0x17,
	0x15, 0xff, 0xff, 0x75, 0xf8, 0x00, 0x00
};

// The same file with the dictionary size field patched to 0x04000000
// (64 MiB). The dictionary size only affects the decoder's window
// allocation, not the compressed data itself.
static const uint8_t big_dict_file[] = {
	0x5d, 0x00, 0x00, 0x00, 0x04, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0x00, 0x24, 0x19, 0x49, 0x98, 0x6f, 0x05,
	0x15, 0x27, 0x27, 0x0d, 0x76, 0x78, 0xd0, 0x2a, 0x68, 0x17,
	0x15, 0xff, 0xff, 0x75, 0xf8, 0x00, 0x00
};


static lzma_ret
decode_all(lzma_stream *strm, const uint8_t *input, size_t input_size)
{
	uint8_t out[512];
	lzma_ret ret;

	strm->next_in = input;
	strm->avail_in = input_size;

	do {
		strm->next_out = out;
		strm->avail_out = sizeof(out);

		ret = lzma_code(strm, LZMA_FINISH);
	} while (ret == LZMA_OK);

	return ret;
}


int
main(void)
{
	lzma_stream strm = LZMA_STREAM_INIT;
	strm.allocator = &my_allocator;

	// Pass 1: decode with a 4 KiB dictionary.
	if (lzma_alone_decoder(&strm, UINT64_MAX) != LZMA_OK)
		return 1;

	if (decode_all(&strm, small_dict_file,
			sizeof(small_dict_file)) != LZMA_STREAM_END)
		return 2;

	// Pass 2: reinitialize and fail the 64 MiB dictionary allocation.
	if (lzma_alone_decoder(&strm, UINT64_MAX) != LZMA_OK)
		return 3;

	if (decode_all(&strm, big_dict_file,
			sizeof(big_dict_file)) != LZMA_MEM_ERROR)
		return 4;

	// Pass 3: reinitialize and decode the 4 KiB-dictionary file again.
	if (lzma_alone_decoder(&strm, UINT64_MAX) != LZMA_OK)
		return 5;

	const lzma_ret ret = decode_all(&strm, small_dict_file,
			sizeof(small_dict_file));
	if (ret != LZMA_STREAM_END)
		return 6;

	lzma_end(&strm);
	return 0;
}
