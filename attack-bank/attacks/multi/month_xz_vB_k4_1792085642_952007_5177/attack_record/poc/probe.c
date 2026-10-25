// SPDX-License-Identifier: 0BSD
//
// Probe: lzma_lzip_decoder() reuse after a failed dictionary allocation.
//
// Drives the public liblzma API with in-contract input:
//   1. Decode the header of a .lz member that specifies a 4 KiB dictionary.
//   2. Re-initialize the same lzma_stream and feed a header that specifies
//      an 8 MiB dictionary; the dictionary allocation fails (custom
//      allocator) and lzma_code() returns LZMA_MEM_ERROR.
//   3. Re-initialize the same lzma_stream again and feed the 4 KiB header.
//
// On a tree where the failed initialization isn't cleaned up AND the
// dictionary size isn't reset, step 3 skips the reallocation and the
// decoder uses the stale state (dict.buf == NULL) -> crash under
// AddressSanitizer.

#include "lzma.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define MEMLIMIT (16U << 20)
#define TOO_BIG_ALLOC (1U << 20)


static void *
my_alloc(void *opaque, size_t nmemb, size_t size)
{
	(void)opaque;
	(void)nmemb;

	// Fail big allocations (the 8 MiB dictionary), succeed the rest.
	if (size >= TOO_BIG_ALLOC)
		return NULL;

	return malloc(size);
}


static lzma_allocator my_allocator = { &my_alloc, NULL, NULL };


// .lz version 0 header: "LZIP", version byte, dictionary-size byte.
// Dictionary size = 2^(ds & 0x1F) - (ds >> 5) * 2^((ds & 0x1F) - 4)
static const uint8_t small_dict_header[6] = {
	'L', 'Z', 'I', 'P', 0x00, 0x0C // 4 KiB dictionary
};

static const uint8_t big_dict_header[6] = {
	'L', 'Z', 'I', 'P', 0x00, 0x17 // 8 MiB dictionary
};

// Some LZMA1 payload; it only needs to get the decoder started.
static const uint8_t payload[64] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};


static lzma_ret
feed(lzma_stream *strm, const uint8_t *in, size_t in_size,
		uint8_t *out, size_t out_size, lzma_action action)
{
	strm->next_in = in;
	strm->avail_in = in_size;
	strm->next_out = out;
	strm->avail_out = out_size;
	return lzma_code(strm, action);
}


int
main(void)
{
	uint8_t out[512];
	lzma_stream strm = LZMA_STREAM_INIT;
	strm.allocator = &my_allocator;

	// Step 1: Initialize with a 4 KiB dictionary. The dictionary
	// allocation succeeds and coder->dict.size is set accordingly.
	if (lzma_lzip_decoder(&strm, MEMLIMIT, 0) != LZMA_OK) {
		printf("init failed\n");
		return 1;
	}

	lzma_ret ret = feed(&strm, small_dict_header,
			sizeof(small_dict_header), out, sizeof(out),
			LZMA_RUN);
	printf("step1 ret=%d\n", ret);

	// Step 2: Re-initialize the same lzma_stream. The 8 MiB dictionary
	// allocation fails and lzma_code() returns LZMA_MEM_ERROR.
	if (lzma_lzip_decoder(&strm, MEMLIMIT, 0) != LZMA_OK) {
		printf("reinit failed\n");
		return 1;
	}

	ret = feed(&strm, big_dict_header, sizeof(big_dict_header),
			out, sizeof(out), LZMA_RUN);
	printf("step2 ret=%d (want %d = LZMA_MEM_ERROR)\n",
			ret, LZMA_MEM_ERROR);

	// Step 3: Re-initialize again with the 4 KiB dictionary. If the
	// failed initialization wasn't cleaned up and the dictionary size
	// wasn't reset, the reallocation is skipped and decoding uses the
	// stale dictionary pointer.
	if (lzma_lzip_decoder(&strm, MEMLIMIT, 0) != LZMA_OK) {
		printf("reinit2 failed\n");
		return 1;
	}

	uint8_t in2[6 + 64];
	memcpy(in2, small_dict_header, 6);
	memcpy(in2 + 6, payload, sizeof(payload));

	ret = feed(&strm, in2, sizeof(in2), out, sizeof(out), LZMA_RUN);
	printf("step3 ret=%d\n", ret);

	lzma_end(&strm);
	printf("no crash\n");
	return 0;
}
