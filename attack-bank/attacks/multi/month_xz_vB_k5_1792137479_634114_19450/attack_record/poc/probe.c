#include <lzma.h>
#include <stdint.h>
#include <stdlib.h>

/* One past the default Index group size. Decoding this Index and
 * duplicating the result exercises group allocation and duplication. */
enum { RECORD_COUNT = 513 };

int
main(void)
{
	lzma_index *built = lzma_index_init(NULL);
	if (built == NULL)
		return 1;

	for (int i = 0; i < RECORD_COUNT; ++i) {
		if (lzma_index_append(built, NULL, 5, 1) != LZMA_OK)
			return 1;
	}

	const size_t need = lzma_index_size(built);
	uint8_t *buf = malloc(need);
	if (buf == NULL)
		return 1;

	size_t out_pos = 0;
	if (lzma_index_buffer_encode(built, buf, &out_pos, need) != LZMA_OK)
		return 1;
	lzma_index_end(built, NULL);

	lzma_index *decoded = NULL;
	uint64_t memlimit = UINT64_MAX;
	size_t in_pos = 0;
	if (lzma_index_buffer_decode(&decoded, &memlimit, NULL,
			buf, &in_pos, out_pos) != LZMA_OK)
		return 1;
	free(buf);

	if (lzma_index_block_count(decoded) != RECORD_COUNT)
		return 1;

	lzma_index *copy = lzma_index_dup(decoded, NULL);
	if (copy == NULL)
		return 1;
	if (lzma_index_block_count(copy) != RECORD_COUNT)
		return 1;

	lzma_index_end(copy, NULL);
	lzma_index_end(decoded, NULL);
	return 0;
}
