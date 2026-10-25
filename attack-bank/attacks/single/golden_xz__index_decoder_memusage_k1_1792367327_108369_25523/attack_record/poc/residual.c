#include <stdio.h>
#include <stdint.h>
#include "lzma.h"

int main(void)
{
	lzma_index *index = NULL;
	uint64_t memlimit = UINT64_MAX;
	size_t in_pos = 0;
	static const uint8_t input[] = {
		0x00, 0x01, 0x05, 0x00,
		0x6e, 0x41, 0xf1, 0x5d,
	};

	const lzma_ret result = lzma_index_buffer_decode(&index,
			&memlimit, NULL, input, &in_pos, sizeof(input));
	if (result != LZMA_OK) {
		fprintf(stderr, "lzma_index_buffer_decode: %d\n", (int)result);
		return 1;
	}

	if (lzma_index_append(index, NULL, 5, 1) != LZMA_OK)
		return 1;

	lzma_index_end(index, NULL);
	return 0;
}
