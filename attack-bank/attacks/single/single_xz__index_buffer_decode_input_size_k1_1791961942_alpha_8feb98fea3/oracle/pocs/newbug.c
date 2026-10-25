// SPDX-License-Identifier: 0BSD

#include "lzma.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int
main(void)
{
	// The first four bytes are already consumed when the decoding
	// begins. What is left is an Index without the CRC32 field.
	const size_t in_size = 8;
	uint8_t *in = malloc(in_size);
	if (in == NULL)
		return 1;

	memset(in, 0xAA, 4);

	in[4] = 0x00;		// Index Indicator
	in[5] = 0x00;		// Number of Records
	in[6] = 0x00;		// Index Padding
	in[7] = 0x00;		// Index Padding

	size_t in_pos = 4;
	uint64_t memlimit = UINT64_C(1) << 30;
	lzma_index *i = NULL;

	const lzma_ret ret = lzma_index_buffer_decode(&i, &memlimit, NULL,
			in, &in_pos, in_size);

	free(in);

	// The Index is truncated: it must be rejected as corrupt data.
	if (ret != LZMA_DATA_ERROR) {
		fprintf(stderr, "unexpected lzma_ret %d\n", (int)ret);
		return 1;
	}

	lzma_index_end(i, NULL);
	return 0;
}
