#include <lzma.h>
#include <stdint.h>
#include <stdio.h>

int
main(void)
{
	lzma_index *first = lzma_index_init(NULL);
	lzma_index *second = lzma_index_init(NULL);
	if (first == NULL || second == NULL)
		return 2;

	lzma_stream_flags flags = {
		.version = 0,
		.backward_size = lzma_index_size(first),
		.check = LZMA_CHECK_CRC32,
	};
	if (lzma_index_stream_flags(first, &flags) != LZMA_OK)
		return 3;

	flags.check = LZMA_CHECK_CRC64;
	if (lzma_index_stream_flags(second, &flags) != LZMA_OK)
		return 4;

	if (lzma_index_cat(first, second, NULL) != LZMA_OK)
		return 5;

	lzma_index *copy = lzma_index_dup(first, NULL);
	if (copy == NULL)
		return 6;

	printf("%u\n", lzma_index_checks(copy));
	lzma_index_end(copy, NULL);
	lzma_index_end(first, NULL);
	return 0;
}
