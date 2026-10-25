#include <stdio.h>

#include "lzma.h"

int
main(void)
{
	lzma_index *index = lzma_index_init(NULL);
	if (index == NULL)
		return 1;

	for (lzma_vli record = 0; record < 513; ++record) {
		if (lzma_index_append(index, NULL, 55, 1) != LZMA_OK)
			return 2;
	}

	lzma_index *copy = lzma_index_dup(index, NULL);
	if (copy == NULL)
		return 3;

	lzma_index_iter iter;
	lzma_index_iter_init(&iter, copy);
	if (lzma_index_iter_next(&iter, LZMA_INDEX_ITER_ANY))
		return 4;

	printf("iteration succeeded\n");

	lzma_index_end(copy, NULL);
	lzma_index_end(index, NULL);
	return 0;
}
