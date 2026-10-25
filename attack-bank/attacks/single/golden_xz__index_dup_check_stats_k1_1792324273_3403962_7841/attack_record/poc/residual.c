// Triggers a heap-buffer-overflow in lzma_index_dup() when the
// duplicated Index contains a Stream whose Records span multiple
// Record groups (more than 2 * 512 Blocks with the defaults).
//
// Uses only the public liblzma API with in-contract arguments.
#include <lzma.h>
#include <stdio.h>

int
main(void)
{
	lzma_index *idx = lzma_index_init(NULL);
	if (idx == NULL)
		return 1;

	// Append enough Blocks that the Records of the Stream span
	// three Record groups.
	for (uint32_t n = 0; n < 1025; n++) {
		if (lzma_index_append(idx, NULL, 5, 1) != LZMA_OK) {
			fprintf(stderr, "lzma_index_append failed\n");
			return 1;
		}
	}

	lzma_index *copy = lzma_index_dup(idx, NULL);
	if (copy == NULL)
		return 1;

	lzma_index_end(copy, NULL);
	lzma_index_end(idx, NULL);

	printf("no crash\n");
	return 0;
}
