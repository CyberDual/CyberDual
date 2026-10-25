#include <lzma.h>

#include <stdint.h>
#include <stddef.h>

int
main(void)
{
	lzma_index *input = lzma_index_init(NULL);
	if (input == NULL)
		return 1;

	if (lzma_index_append(input, NULL, 16, 1) != LZMA_OK) {
		lzma_index_end(input, NULL);
		return 1;
	}

	uint8_t encoded[64];
	size_t encoded_size = 0;
	if (lzma_index_buffer_encode(input, encoded, &encoded_size,
			sizeof(encoded)) != LZMA_OK) {
		lzma_index_end(input, NULL);
		return 1;
	}

	lzma_index_end(input, NULL);

	lzma_index *decoded = NULL;
	uint64_t memlimit = UINT64_C(1) << 20;
	size_t decoded_size = 0;
	if (lzma_index_buffer_decode(&decoded, &memlimit, NULL, encoded,
			&decoded_size, encoded_size) != LZMA_OK)
		return 1;

	lzma_index *copy = lzma_index_dup(decoded, NULL);
	lzma_index_end(decoded, NULL);
	if (copy == NULL)
		return 1;

	if (lzma_index_append(copy, NULL, 16, 1) != LZMA_OK) {
		lzma_index_end(copy, NULL);
		return 1;
	}

	if (lzma_index_append(copy, NULL, 16, 1) != LZMA_OK) {
		lzma_index_end(copy, NULL);
		return 1;
	}

	lzma_index_end(copy, NULL);
	return 0;
}
