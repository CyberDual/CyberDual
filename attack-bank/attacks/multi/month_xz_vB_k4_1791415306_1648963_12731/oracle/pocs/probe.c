#include <lzma.h>

int main(void)
{
	lzma_index *first = lzma_index_init(NULL);
	lzma_index *second = lzma_index_init(NULL);
	lzma_index *third = lzma_index_init(NULL);
	if (first == NULL || second == NULL || third == NULL)
		return 1;
	if (lzma_index_stream_padding(first, 4) != LZMA_OK)
		return 2;
	if (lzma_index_cat(first, second, NULL) != LZMA_OK)
		return 3;
	if (lzma_index_cat(first, third, NULL) != LZMA_OK)
		return 4;
	lzma_index_end(first, NULL);
	return 0;
}
