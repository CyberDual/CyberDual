// SPDX-License-Identifier: 0BSD
//
// Proof-of-concept driver for the delocalized liblzma memory-safety defect.
//
// It uses only the public liblzma decoder API (lzma_microlzma_decoder() and
// lzma_code()) together with a custom lzma_allocator, which is a documented,
// in-contract way to use the library. The allocator refuses allocations that
// are "too big", emulating an out-of-memory situation for a large dictionary.
//
// MicroLZMA takes the LZMA dictionary size as an ordinary API argument, so the
// three decoder passes below only differ in that argument and in a one-byte
// input holding the LZMA properties:
//
//   1. Decode with a small (4 KiB) dictionary. The internal dictionary buffer
//      is allocated while initializing the LZMA filter.
//   2. Reuse the same lzma_stream with a large (8 MiB) dictionary. The
//      dictionary (re)allocation fails, so lzma_code() returns LZMA_MEM_ERROR.
//   3. Reuse the same lzma_stream once more with the small dictionary.
//
// On a correctly fixed liblzma this all works and the program exits 0. When
// the reinitialization-after-allocation-failure handling is incomplete,
// step 3 reuses a dictionary buffer that was released in step 2, which
// AddressSanitizer reports as a fatal memory error.

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "lzma.h"

// Ensure AddressSanitizer turns any detected error into an abort() (SIGABRT),
// so the failure is observable as a non-zero/aborting exit even if the
// environment does not set ASAN_OPTIONS. This only makes the sanitizer
// stricter; it does not affect liblzma's behavior.
const char *__asan_default_options(void);
const char *
__asan_default_options(void)
{
	return "abort_on_error=1";
}

#define SMALL_DICT (4U << 10)   // 4 KiB
#define BIG_DICT   (8U << 20)   // 8 MiB
#define TOO_BIG_ALLOC (1U << 20)

static void *
my_alloc(void *opaque, size_t nmemb, size_t size)
{
	(void)opaque;
	(void)nmemb;

	// Emulate running out of memory for large allocations such as the
	// 8 MiB dictionary buffer. Small allocations still succeed.
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

// MicroLZMA reads the LZMA properties byte as the bitwise negation of the
// usual encoding. ~0xA2 == 0x5D encodes the default lc=3, lp=0, pb=2.
static const uint8_t props_in[] = { 0xA2 };

static int
run_pass(lzma_stream *strm, uint32_t dict_size, lzma_ret expected)
{
	uint8_t out[64];

	if (lzma_microlzma_decoder(strm, /*comp_size=*/ sizeof(props_in),
			/*uncomp_size=*/ 1, /*uncomp_size_is_exact=*/ 0,
			dict_size) != LZMA_OK)
		return 1;

	strm->next_in = props_in;
	strm->avail_in = sizeof(props_in);
	strm->next_out = out;
	strm->avail_out = sizeof(out);

	const lzma_ret ret = lzma_code(strm, LZMA_FINISH);
	return ret == expected ? 0 : 1;
}

int
main(void)
{
	lzma_stream strm = LZMA_STREAM_INIT;
	strm.allocator = &my_allocator;

	// Step 1: small dictionary; allocates the dictionary buffer.
	if (run_pass(&strm, SMALL_DICT, LZMA_OK))
		return 2;

	// Step 2: large dictionary; dictionary allocation fails.
	if (run_pass(&strm, BIG_DICT, LZMA_MEM_ERROR))
		return 2;

	// Step 3: small dictionary again, reusing the same lzma_stream.
	if (run_pass(&strm, SMALL_DICT, LZMA_OK))
		return 2;

	lzma_end(&strm);

	fprintf(stderr, "probe completed without memory error\n");
	return 0;
}
