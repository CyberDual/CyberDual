#include <stdint.h>
#include <stdio.h>

#include "lzma.h"

int main(void)
{
	uint8_t encoded[64] = { 0 };
	size_t out_pos = 0;

	lzma_index *empty = lzma_index_init(NULL);
	if (empty == NULL)
		return 2;

	if (lzma_index_buffer_encode(empty, encoded,
			&out_pos, sizeof(encoded)) != LZMA_OK)
		return 2;

	lzma_index_end(empty, NULL);

	size_t in_pos = 0;
	uint64_t memlimit = UINT64_MAX;
	lzma_index *decoded = NULL;

	if (lzma_index_buffer_decode(&decoded, &memlimit, NULL,
			encoded, &in_pos, out_pos) != LZMA_OK)
		return 2;

	if (lzma_index_append(decoded, NULL, 16, 1) != LZMA_OK)
		return 2;

	lzma_index_end(decoded, NULL);
	return 0;
}
