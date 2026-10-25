#include <lzma.h>

#include <stdint.h>
#include <stdlib.h>

int
main(void)
{
	lzma_index *idx = lzma_index_init(NULL);
	if (idx == NULL)
		return 1;

	for (int i = 0; i < 4; ++i) {
		if (lzma_index_append(idx, NULL, 5, 1) != LZMA_OK)
			return 1;
	}

	const lzma_vli index_size = lzma_index_size(idx);
	uint8_t *buf = malloc((size_t)index_size);
	if (buf == NULL)
		return 1;

	size_t pos = 0;
	if (lzma_index_buffer_encode(idx, buf, &pos, (size_t)index_size)
			!= LZMA_OK)
		return 1;

	lzma_index_end(idx, NULL);

	lzma_index *dec = NULL;
	uint64_t memlimit = UINT64_MAX;
	size_t in_pos = 0;
	if (lzma_index_buffer_decode(&dec, &memlimit, NULL,
			buf, &in_pos, pos) != LZMA_OK)
		return 1;

	/* Appending one Record to a decoded Index is a supported use. */
	if (lzma_index_append(dec, NULL, 64, 10) != LZMA_OK)
		return 1;

	lzma_index_end(dec, NULL);
	free(buf);
	return 0;
}
