#include <lzma.h>
#include <stdint.h>
#include <stdlib.h>

int
main(void)
{
	uint8_t *input = malloc(1);
	if (input == NULL)
		return 2;

	input[0] = 0;
	size_t input_pos = 1;
	uint64_t memlimit = 1 << 20;
	lzma_index *index = NULL;
	lzma_ret ret = lzma_index_buffer_decode(&index, &memlimit, NULL,
			input, &input_pos, 1);
	lzma_index_end(index, NULL);
	free(input);
	return ret == LZMA_DATA_ERROR && input_pos == 1 ? 0 : 1;
}
