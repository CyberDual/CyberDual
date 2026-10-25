// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
// \file       probe.c
// \brief      lzma_alone_decoder() reuse test with a failing allocator
//
// Exercises the public API: lzma_alone_decoder(), lzma_code(), lzma_end(),
// lzma_alone_encoder(), lzma_lzma_preset(), and a custom lzma_allocator
// that rejects large allocations.
//
///////////////////////////////////////////////////////////////////////////////

#include "lzma.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define MEMLIMIT (32U << 20)
#define TOO_BIG_ALLOC (1U << 20)


// A custom allocator that simulates running out of memory for large
// allocations. Using custom allocators this way is part of the liblzma
// API contract (lzma_stream.allocator).
static void *
my_alloc(void *opaque, size_t nmemb, size_t size)
{
	(void)opaque;

	if (size != 0 && nmemb > SIZE_MAX / size)
		return NULL;

	if (nmemb * size >= TOO_BIG_ALLOC)
		return NULL;

	return malloc(nmemb * size);
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


static uint8_t small_file[128 << 10];
static size_t small_file_size;

static uint8_t big_file[128 << 10];
static size_t big_file_size;

static uint8_t out_buf[64 << 10];


// Build a valid .lzma (LZMA_Alone) file with a 4 KiB dictionary using the
// public encoder API. The payload is 8 KiB of incompressible data so that
// decoding writes over 8 KiB into the dictionary.
static void
make_input(void)
{
	uint8_t data[8192];
	uint32_t x = 123456789U;
	for (size_t i = 0; i < sizeof(data); ++i) {
		x = x * 1103515245U + 12345U;
		data[i] = (uint8_t)(x >> 16);
	}

	lzma_options_lzma options;
	if (lzma_lzma_preset(&options, 1)) {
		fprintf(stderr, "lzma_lzma_preset() failed\n");
		exit(10);
	}

	options.dict_size = 4096;

	lzma_stream estrm = LZMA_STREAM_INIT;
	if (lzma_alone_encoder(&estrm, &options) != LZMA_OK) {
		fprintf(stderr, "lzma_alone_encoder() failed\n");
		exit(11);
	}

	estrm.next_in = data;
	estrm.avail_in = sizeof(data);
	estrm.next_out = small_file;
	estrm.avail_out = sizeof(small_file);

	lzma_ret ret;
	do {
		ret = lzma_code(&estrm, LZMA_FINISH);
		if (ret != LZMA_OK && ret != LZMA_STREAM_END) {
			fprintf(stderr, "encoding failed: %d\n", ret);
			exit(12);
		}
	} while (ret != LZMA_STREAM_END);

	small_file_size = sizeof(small_file) - estrm.avail_out;
	lzma_end(&estrm);

	if (small_file_size < 13 || small_file_size >= sizeof(big_file)) {
		fprintf(stderr, "unexpected encoded size\n");
		exit(13);
	}

	// Make a second file whose header declares an 8 MiB dictionary.
	// The header is: properties (1 byte), dictionary size (4 bytes,
	// little endian), uncompressed size (8 bytes).
	memcpy(big_file, small_file, small_file_size);
	big_file_size = small_file_size;
	big_file[2] = 0x00;
	big_file[3] = 0x80;
}


int
main(void)
{
	make_input();

	// The same lzma_stream is reused for all three decoding attempts:
	// lzma_alone_decoder() reinitializes the existing decoder object
	// instead of freeing and reallocating it.
	lzma_stream strm = LZMA_STREAM_INIT;
	strm.allocator = &my_allocator;

	// 1st: A 4 KiB dictionary decodes fine even with the failing
	// allocator. The decoder allocates and keeps a small dictionary.
	if (lzma_alone_decoder(&strm, MEMLIMIT) != LZMA_OK) {
		fprintf(stderr, "lzma_alone_decoder() failed\n");
		return 20;
	}
	strm.next_in = small_file;
	strm.avail_in = small_file_size;
	strm.next_out = out_buf;
	strm.avail_out = sizeof(out_buf);
	if (lzma_code(&strm, LZMA_FINISH) != LZMA_STREAM_END) {
		fprintf(stderr, "1st decode failed\n");
		lzma_end(&strm);
		return 1;
	}

	// 2nd: Reinitialize the same stream with an 8 MiB dictionary.
	// The dictionary allocation fails and the call must report it.
	if (lzma_alone_decoder(&strm, MEMLIMIT) != LZMA_OK) {
		fprintf(stderr, "lzma_alone_decoder() failed\n");
		return 21;
	}
	strm.next_in = big_file;
	strm.avail_in = big_file_size;
	strm.next_out = out_buf;
	strm.avail_out = sizeof(out_buf);
	if (lzma_code(&strm, LZMA_FINISH) != LZMA_MEM_ERROR) {
		fprintf(stderr, "2nd decode did not fail with MEM_ERROR\n");
		lzma_end(&strm);
		return 2;
	}

	// 3rd: Retry with the same 8 MiB dictionary file. This must either
	// fail cleanly or decode correctly, without memory errors.
	if (lzma_alone_decoder(&strm, MEMLIMIT) != LZMA_OK) {
		fprintf(stderr, "lzma_alone_decoder() failed\n");
		return 22;
	}
	strm.next_in = big_file;
	strm.avail_in = big_file_size;
	strm.next_out = out_buf;
	strm.avail_out = sizeof(out_buf);
	const lzma_ret ret = lzma_code(&strm, LZMA_FINISH);
	(void)ret;

	lzma_end(&strm);
	return 0;
}
