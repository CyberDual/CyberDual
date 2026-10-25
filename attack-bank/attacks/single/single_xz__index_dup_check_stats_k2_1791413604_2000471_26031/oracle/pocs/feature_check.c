// Sanity check: lzma_index_dup() must preserve the full Check bitmask
// of a multi-Stream Index, not just the last Stream's Check type.
#include <lzma.h>
#include <stdio.h>
#include <assert.h>

int main(void)
{
	lzma_index *idx = lzma_index_init(NULL);
	lzma_stream_flags flags = {
		.version = 0,
		.backward_size = LZMA_BACKWARD_SIZE_MIN,
		.check = LZMA_CHECK_CRC32,
	};
	assert(lzma_index_stream_flags(idx, &flags) == LZMA_OK);

	lzma_index *second = lzma_index_init(NULL);
	flags.check = LZMA_CHECK_SHA256;
	assert(lzma_index_stream_flags(second, &flags) == LZMA_OK);
	assert(lzma_index_cat(idx, second, NULL) == LZMA_OK);

	uint32_t orig_checks = lzma_index_checks(idx);

	lzma_index *copy = lzma_index_dup(idx, NULL);
	assert(copy != NULL);

	uint32_t copy_checks = lzma_index_checks(copy);

	printf("orig=%u copy=%u\n", orig_checks, copy_checks);
	assert(copy_checks == orig_checks);
	assert(copy_checks == (LZMA_INDEX_CHECK_MASK_CRC32
			| LZMA_INDEX_CHECK_MASK_SHA256));

	lzma_index_end(idx, NULL);
	lzma_index_end(copy, NULL);

	printf("OK\n");
	return 0;
}
