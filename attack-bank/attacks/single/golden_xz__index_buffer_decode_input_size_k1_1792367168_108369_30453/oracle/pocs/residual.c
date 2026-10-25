#include <lzma.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int
main(void)
{
	lzma_index *index = lzma_index_init(NULL);
	if (index == NULL)
		return 1;

	if (lzma_index_append(index, NULL, 55, 1) != LZMA_OK)
		return 1;

	uint8_t encoded[512];
	size_t encoded_size = 0;
	if (lzma_index_buffer_encode(index, encoded, &encoded_size,
			sizeof(encoded)) != LZMA_OK)
		return 1;

	lzma_index_end(index, NULL);

	const size_t prefix_size = 8;
	uint8_t *input = malloc(prefix_size + encoded_size - 1);
	if (input == NULL)
		return 1;

	memset(input, 0, prefix_size);
	memcpy(input + prefix_size, encoded, encoded_size - 1);

	lzma_index *decoded = NULL;
	uint64_t memlimit = UINT64_MAX;
	size_t in_pos = prefix_size;
	const lzma_ret ret = lzma_index_buffer_decode(&decoded, &memlimit,
			NULL, input, &in_pos, prefix_size + encoded_size - 1);

	printf("ret=%d in_pos=%zu\n", ret, in_pos);
	lzma_index_end(decoded, NULL);
	free(input);
	return ret == LZMA_DATA_ERROR ? 0 : 1;
}
