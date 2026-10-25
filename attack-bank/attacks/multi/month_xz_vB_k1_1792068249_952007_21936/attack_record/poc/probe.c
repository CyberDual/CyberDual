// SPDX-License-Identifier: 0BSD

// Probe for the code-review benchmark: drives liblzma's public .lz (lzip)
// decoder API the same way tests/test_alone_decoder.c drives the .lzma
// decoder, i.e. with a custom lzma_allocator whose big allocations fail.
//
//   1. Decode the header of a .lz member that specifies a 4 KiB dictionary.
//      The dictionary buffer is allocated and the member's LZMA1 sub-decoder
//      is initialized.
//   2. Re-initialize the decoder and decode the header of a member that
//      specifies an 8 MiB dictionary. The dictionary reallocation fails
//      (the custom allocator refuses it) and lzma_code() returns
//      LZMA_MEM_ERROR.
//   3. Re-initialize the decoder and decode the 4 KiB header again.
//
// On a broken tree, step 3 reuses the sub-decoder whose dictionary state
// still refers to the freed dictionary and the decoder writes through the
// stale dictionary pointer.

#include "lzma.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// The custom allocator refuses allocations of this size and bigger.
// This is the same threshold as in tests/test_alone_decoder.c.
#define TOO_BIG_ALLOC (1U << 20)


static void *
my_alloc(void *opaque lzma_attribute((__unused__)),
		size_t nmemb, size_t size)
{
	if (size >= TOO_BIG_ALLOC || nmemb >= TOO_BIG_ALLOC)
		return NULL;

	return malloc(nmemb * size);
}


static void
my_free(void *opaque lzma_attribute((__unused__)), void *ptr)
{
	free(ptr);
	return;
}


static lzma_allocator my_allocator = {
	.alloc = &my_alloc,
	.free = &my_free,
	.opaque = NULL,
};


// Headers of two .lz members. The first six bytes of a member are
// the ID string "LZIP", the format version, and the dictionary
// size byte. 0x0C means a 4 KiB dictionary and 0x17 means 8 MiB.
static const uint8_t small_dict_header[] = {
	0x4C, 0x5A, 0x49, 0x50, 0x01, 0x0C
};

static const uint8_t big_dict_header[] = {
	0x4C, 0x5A, 0x49, 0x50, 0x01, 0x17
};


static lzma_ret
feed(lzma_stream *strm, const uint8_t *in, size_t in_size, const char *what)
{
	uint8_t out[512];

	strm->next_in = in;
	strm->avail_in = in_size;
	strm->next_out = out;
	strm->avail_out = sizeof(out);

	const lzma_ret ret = lzma_code(strm, LZMA_RUN);
	printf("%s: lzma_code() returned %d\n", what, ret);
	return ret;
}


int
main(void)
{
	lzma_stream strm = LZMA_STREAM_INIT;
	strm.allocator = &my_allocator;

	// Step 1: A member with a 4 KiB dictionary initializes the
	// LZMA1 sub-decoder and its dictionary.
	if (lzma_lzip_decoder(&strm, UINT64_MAX, 0) != LZMA_OK) {
		printf("lzma_lzip_decoder() failed\n");
		return 1;
	}

	feed(&strm, small_dict_header, sizeof(small_dict_header), "step 1");

	// Step 2: A member with an 8 MiB dictionary. The dictionary
	// reallocation fails.
	if (lzma_lzip_decoder(&strm, UINT64_MAX, 0) != LZMA_OK) {
		printf("lzma_lzip_decoder() failed\n");
		return 1;
	}

	feed(&strm, big_dict_header, sizeof(big_dict_header), "step 2");

	// Step 3: A member with a 4 KiB dictionary again.
	if (lzma_lzip_decoder(&strm, UINT64_MAX, 0) != LZMA_OK) {
		printf("lzma_lzip_decoder() failed\n");
		return 1;
	}

	feed(&strm, small_dict_header, sizeof(small_dict_header), "step 3");

	lzma_end(&strm);

	printf("no crash\n");
	return 0;
}
