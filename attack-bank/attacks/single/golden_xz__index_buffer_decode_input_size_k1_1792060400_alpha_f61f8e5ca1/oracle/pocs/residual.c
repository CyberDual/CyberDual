#include <lzma.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
 * Truncated Index placed at a non-zero in_pos. Number of Records fits in
 * the unread tail (each Record is at least two bytes), but the Record list
 * itself runs past the end of the buffer.
 */
int
main(void)
{
	const size_t in_size = 24;
	const size_t in_start = 8;
	uint8_t *in = malloc(in_size);
	if (in == NULL)
		return 1;

	memset(in, 0x11, in_size);
	in[in_start] = 0x00;
	in[in_start + 1] = 0x08;
	for (size_t i = 0; i < 7; ++i) {
		in[in_start + 2 + 2 * i] = 0x05;
		in[in_start + 3 + 2 * i] = 0x01;
	}

	size_t in_pos = in_start;
	uint64_t memlimit = UINT64_C(1) << 30;
	lzma_index *idx = NULL;
	lzma_ret ret = lzma_index_buffer_decode(&idx, &memlimit, NULL,
			in, &in_pos, in_size);

	lzma_index_end(idx, NULL);
	free(in);
	return ret == LZMA_DATA_ERROR ? 0 : 1;
}
