// SPDX-License-Identifier: 0BSD

#include "lzma.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int
main(void)
{
	// Number of Records is 20000. It is valid only if the input
	// size is calculated from in_size instead of the input that
	// is left after *in_pos.
	const size_t prefix_size = 60000;
	const size_t in_size = prefix_size + 4;
	uint8_t *in = malloc(in_size);
	if (in == NULL)
		return 1;

	memset(in, 0, in_size);
	in[prefix_size + 0] = 0x00; // Index Indicator
	in[prefix_size + 1] = 0xA0; // Number of Records (varint)
	in[prefix_size + 2] = 0x9C;
	in[prefix_size + 3] = 0x01;

	size_t in_pos = prefix_size;
	uint64_t memlimit = 1;
	lzma_index *i = NULL;

	const lzma_ret ret = lzma_index_buffer_decode(&i, &memlimit, NULL,
			in, &in_pos, in_size);

	free(in);

	// The Number of Records is impossible for the remaining input
	// size. The Index must be rejected as corrupt data and not as
	// a memory usage limit problem.
	if (ret != LZMA_DATA_ERROR)
		abort();

	lzma_index_end(i, NULL);
	return 0;
}
