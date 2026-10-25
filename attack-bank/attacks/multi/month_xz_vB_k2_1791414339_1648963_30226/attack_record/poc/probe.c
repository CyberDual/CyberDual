#include <lzma.h>
#include <stdint.h>
#include <stdlib.h>

int
main(void)
{
	lzma_index *source = lzma_index_init(NULL);
	if (source == NULL)
		return 1;

	if (lzma_index_append(source, NULL, 5, 1) != LZMA_OK)
		return 2;

	const size_t size = (size_t)lzma_index_size(source);
	uint8_t *buffer = malloc(size);
	if (buffer == NULL)
		return 3;

	size_t pos = 0;
	if (lzma_index_buffer_encode(source, buffer, &pos, size) != LZMA_OK)
		return 4;
	lzma_index_end(source, NULL);

	lzma_index *decoded = NULL;
	uint64_t memlimit = UINT64_MAX;
	pos = 0;
	if (lzma_index_buffer_decode(&decoded, &memlimit, NULL,
			buffer, &pos, size) != LZMA_OK)
		return 5;
	free(buffer);

	lzma_index *copy = lzma_index_dup(decoded, NULL);
	if (copy == NULL)
		return 6;
	lzma_index_end(copy, NULL);
	lzma_index_end(decoded, NULL);
	return 0;
}
