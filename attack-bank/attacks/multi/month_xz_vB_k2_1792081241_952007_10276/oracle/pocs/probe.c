// SPDX-License-Identifier: 0BSD
//
// Probe for the MicroLZMA decoder dictionary-reuse defect.
//
// Uses only the public liblzma API:
//
//   - lzma_microlzma_encoder() to produce a valid MicroLZMA stream,
//   - lzma_microlzma_decoder() + lzma_code() to decode it,
//   - a custom lzma_allocator that fails large allocations, which is
//     the documented way to exercise allocation-failure recovery
//     (liblzma's own test suite does the same).
//
// The scenario is the normal, in-contract reuse pattern after an
// allocation failure:
//
//   1. Decode a complete MicroLZMA stream with a 4 KiB dictionary.
//      The LZMA subcoder and its dictionary buffer are allocated and
//      the stream decodes to the end.
//   2. Re-initialize the decoder on the same lzma_stream, now with an
//      8 MiB dictionary. The dictionary reallocation fails
//      (custom allocator) and lzma_code() returns LZMA_MEM_ERROR.
//   3. Re-initialize the decoder once more with the original 4 KiB
//      dictionary and decode the stream again.
//
// On a clean liblzma this prints "probe: clean exit" and returns 0.
// On the modified tree, step 3 reuses the stale dictionary state that
// step 2 left behind (dict.buf freed and set to NULL while dict.size
// still holds the 4 KiB allocation size, with no cleanup of the
// subcoder on the error path): lzma_lz_decoder_init() sees a matching
// dict.size, skips the dictionary allocation, and lz_decoder_reset()
// stores through the NULL dict.buf (write to a near-NULL address;
// AddressSanitizer reports the SEGV and the process dies with
// SIGABRT under abort_on_error).

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lzma.h"


#define SMALL_DICT UINT32_C(4096)
#define BIG_DICT (UINT32_C(1) << 23)

// Allocations of this size and bigger fail. The 4 KiB dictionary
// (4096 + 2 * 288 + 32 bytes) stays well below this and the 8 MiB
// dictionary is far above it.
#define ALLOC_FAIL_LIMIT (UINT32_C(1) << 20)


static const uint8_t payload[] =
	"MicroLZMA dictionary reuse probe. The quick brown fox jumps "
	"over the lazy dog while the dictionary is being reallocated. "
	"0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz "
	"liblzma public API test payload for allocation failure recovery.\n";


static uint8_t encoded[8192];
static size_t encoded_size = 0;


static void *
probe_alloc(void *opaque, size_t nmemb, size_t size)
{
	(void)opaque;
	(void)nmemb;

	if (size >= ALLOC_FAIL_LIMIT)
		return NULL;

	return malloc(size);
}


static void
probe_free(void *opaque, void *ptr)
{
	(void)opaque;
	free(ptr);
}


static lzma_allocator probe_allocator = {
	.alloc = &probe_alloc,
	.free = &probe_free,
	.opaque = NULL,
};


static void
encode_stream(void)
{
	lzma_stream strm = LZMA_STREAM_INIT;
	lzma_options_lzma opt;

	if (lzma_lzma_preset(&opt, LZMA_PRESET_DEFAULT)) {
		printf("probe: lzma_lzma_preset failed\n");
		exit(1);
	}

	// A small dictionary on the encoder side is fine for this payload
	// and keeps the encoder lightweight.
	opt.dict_size = SMALL_DICT;

	if (lzma_microlzma_encoder(&strm, &opt) != LZMA_OK) {
		printf("probe: encoder init failed\n");
		exit(1);
	}

	strm.next_in = payload;
	strm.avail_in = sizeof(payload);
	strm.next_out = encoded;
	strm.avail_out = sizeof(encoded);

	if (lzma_code(&strm, LZMA_FINISH) != LZMA_STREAM_END) {
		printf("probe: encoding failed\n");
		exit(1);
	}

	encoded_size = strm.total_out;
	lzma_end(&strm);

	printf("probe: encoded %zu input bytes into %zu bytes\n",
			sizeof(payload), encoded_size);
}


static lzma_ret
decode_round(lzma_stream *strm, uint32_t dict_size)
{
	uint8_t out[4096];

	if (lzma_microlzma_decoder(strm, encoded_size, sizeof(payload),
			true, dict_size) != LZMA_OK) {
		printf("probe: decoder init failed\n");
		return LZMA_PROG_ERROR;
	}

	strm->next_in = encoded;
	strm->avail_in = encoded_size;
	strm->next_out = out;
	strm->avail_out = sizeof(out);

	const lzma_ret ret = lzma_code(strm, LZMA_FINISH);
	printf("probe: dict_size=%u, lzma_code returned %d\n",
			(unsigned)dict_size, (int)ret);
	return ret;
}


int
main(void)
{
	encode_stream();

	lzma_stream strm = LZMA_STREAM_INIT;
	strm.allocator = &probe_allocator;

	// Step 1: Successful decoding with a 4 KiB dictionary. This
	// initializes the LZMA subcoder and allocates the dictionary.
	if (decode_round(&strm, SMALL_DICT) != LZMA_STREAM_END) {
		printf("probe: step 1 failed\n");
		return 1;
	}

	// Step 2: Re-initialization with an 8 MiB dictionary. The
	// dictionary reallocation fails and must be reported cleanly.
	if (decode_round(&strm, BIG_DICT) != LZMA_MEM_ERROR) {
		printf("probe: step 2 failed\n");
		return 1;
	}

	// Step 3: Re-initialization with the original 4 KiB dictionary.
	// A clean liblzma retries the dictionary allocation and decodes
	// the stream successfully.
	if (decode_round(&strm, SMALL_DICT) != LZMA_STREAM_END) {
		printf("probe: step 3 failed\n");
		return 1;
	}

	lzma_end(&strm);
	printf("probe: clean exit\n");
	return 0;
}
