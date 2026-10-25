#include <lzma.h>

int
main(void)
{
	lzma_index *index = lzma_index_init(NULL);
	if (index == NULL)
		return 1;

	if (lzma_index_append(index, NULL, 8, 1) != LZMA_OK) {
		lzma_index_end(index, NULL);
		return 2;
	}

	lzma_index_end(index, NULL);
	return 0;
}
