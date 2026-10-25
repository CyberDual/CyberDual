#include <lzma.h>
#include <stdint.h>

int main(void)
{
	uint8_t buffer[64];
	lzma_index *index = lzma_index_init(NULL);
	if (index == NULL)
		return 1;

	size_t buffer_pos = 0;
	if (lzma_index_buffer_encode(index, buffer, &buffer_pos,
			sizeof(buffer)) != LZMA_OK) {
		lzma_index_end(index, NULL);
		return 2;
	}
	lzma_index_end(index, NULL);

	index = NULL;
	uint64_t memlimit = UINT64_MAX;
	size_t input_pos = 0;
	if (lzma_index_buffer_decode(&index, &memlimit, NULL, buffer,
			&input_pos, buffer_pos) != LZMA_OK)
		return 3;

	const lzma_ret ret = lzma_index_append(index, NULL, 55, 1);
	(void)ret;
	lzma_index_end(index, NULL);
	return 0;
}
