#include <lzma.h>
#include <stdlib.h>

static lzma_index *one_stream(void)
{
	lzma_index *index = lzma_index_init(NULL);
	if (index == NULL || lzma_index_append(index, NULL, 32, 16) != LZMA_OK)
		exit(2);
	return index;
}

int main(void)
{
	lzma_index *index = one_stream();
	for (unsigned i = 0; i < 3; ++i) {
		lzma_index *next = one_stream();
		if (lzma_index_cat(next, index, NULL) != LZMA_OK)
			exit(2);
		index = next;
	}

	lzma_index *copy = lzma_index_dup(index, NULL);
	if (copy == NULL)
		exit(2);
	lzma_index_end(index, NULL);

	lzma_index_iter iter;
	lzma_index_iter_init(&iter, copy);
	while (!lzma_index_iter_next(&iter, LZMA_INDEX_ITER_STREAM))
		(void)iter.stream.number;

	lzma_index_end(copy, NULL);
	return 0;
}
