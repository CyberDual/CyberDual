#include <lzma.h>
#include <stdlib.h>

int
main(void)
{
	lzma_index *first = lzma_index_init(NULL);
	lzma_index *second = lzma_index_init(NULL);
	if (first == NULL || second == NULL)
		return 1;

	lzma_stream_flags crc32 = {
		.version = 0,
		.backward_size = LZMA_VLI_UNKNOWN,
		.check = LZMA_CHECK_CRC32
	};
	lzma_stream_flags sha256 = {
		.version = 0,
		.backward_size = LZMA_VLI_UNKNOWN,
		.check = LZMA_CHECK_SHA256
	};

	if (lzma_index_stream_flags(first, &crc32) != LZMA_OK)
		return 1;
	if (lzma_index_stream_flags(second, &sha256) != LZMA_OK)
		return 1;

	for (unsigned i = 0; i < 513; ++i) {
		if (lzma_index_append(first, NULL, 8, 1) != LZMA_OK)
			return 1;
	}
	if (lzma_index_append(second, NULL, 16, 4) != LZMA_OK)
		return 1;

	if (lzma_index_cat(first, second, NULL) != LZMA_OK)
		return 1;

	lzma_index *copy = lzma_index_dup(first, NULL);
	if (copy == NULL)
		return 1;

	lzma_index_end(copy, NULL);
	lzma_index_end(first, NULL);
	return 0;
}
