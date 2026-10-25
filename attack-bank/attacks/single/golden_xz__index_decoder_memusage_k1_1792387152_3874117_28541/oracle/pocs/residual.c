// SPDX-License-Identifier: 0BSD
//
// PoC for the benchmark: drive a fully valid .xz Index through the
// public lzma_index_decoder() API, and then query lzma_memusage() on
// the still-initialized stream after decoding has finished
// (LZMA_STREAM_END returned, lzma_end() not yet called).
//
// The Index bytes are produced by the library's own public Index
// encoder, so the input is an ordinary, in-contract Index.

#include "lzma.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int
main(void)
{
	// Build a valid Index with a handful of Records.
	lzma_index *idx = lzma_index_init(NULL);
	if (idx == NULL)
		return 1;

	for (lzma_vli i = 0; i < 20; ++i)
		if (lzma_index_append(idx, NULL, 32 + i, 512 + i)
				!= LZMA_OK)
			return 1;

	uint8_t buf[4096];
	size_t buf_pos = 0;
	if (lzma_index_buffer_encode(idx, buf, &buf_pos, sizeof(buf))
			!= LZMA_OK)
		return 1;

	lzma_index_end(idx, NULL);

	// Decode it with the public Index decoder.
	lzma_stream strm = LZMA_STREAM_INIT;
	lzma_index *decoded = NULL;

	if (lzma_index_decoder(&strm, &decoded, UINT64_MAX) != LZMA_OK)
		return 1;

	strm.next_in = buf;
	strm.avail_in = buf_pos;

	if (lzma_code(&strm, LZMA_FINISH) != LZMA_STREAM_END)
		return 1;

	printf("Decoded %llu Records from a valid Index.\n",
			(unsigned long long)lzma_index_block_count(decoded));
	printf("Calling lzma_memusage() on the still-open stream...\n");
	fflush(stdout);

	// Legal call on an initialized stream: the memory usage of the
	// Index decoder.
	const uint64_t memusage = lzma_memusage(&strm);
	printf("lzma_memusage() = %llu bytes\n",
			(unsigned long long)memusage);

	lzma_index_end(decoded, NULL);
	lzma_end(&strm);
	return 0;
}
