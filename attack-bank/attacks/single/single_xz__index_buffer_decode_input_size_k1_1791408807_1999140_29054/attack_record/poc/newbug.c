// SPDX-License-Identifier: 0BSD
//
// Drives lzma_index_buffer_decode() through its public API with *in_pos > 0.
// The crafted input is a truncated Index: it declares more Records than the
// bytes available after *in_pos can hold, so a correct decoder must stop at
// in_size and report LZMA_DATA_ERROR without reading past the buffer.
//
// The input buffer is heap-allocated at exactly its real size so that any
// read beyond the declared in_size is caught by AddressSanitizer.

#include <lzma.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(void)
{
	// Layout: 8 arbitrary leading bytes (skipped via in_pos), then the
	// start of an Index that claims 8 Records but is cut short.
	const size_t prefix = 8;
	const size_t avail = 16;            // real Index bytes available
	const size_t size = prefix + avail; // total heap buffer size

	uint8_t *buf = malloc(size);
	if (buf == NULL)
		return 2;

	memset(buf, 0, prefix);

	size_t p = prefix;
	buf[p++] = 0x00;        // Index Indicator
	buf[p++] = 0x08;        // Number of Records = 8

	// Fill the remaining bytes with minimal valid Records
	// (Unpadded Size = 5, Uncompressed Size = 0). Only 7 full Records
	// fit, one byte short of the 8 that were declared.
	while (p < size) {
		buf[p++] = 0x05;
		if (p < size)
			buf[p++] = 0x00;
	}

	lzma_index *idx = NULL;
	uint64_t memlimit = UINT64_MAX;
	size_t in_pos = prefix;

	lzma_ret ret = lzma_index_buffer_decode(&idx, &memlimit, NULL,
			buf, &in_pos, size);

	// A correct implementation returns here with LZMA_DATA_ERROR.
	printf("ret=%d in_pos=%zu\n", (int)ret, in_pos);

	lzma_index_end(idx, NULL);
	free(buf);
	return 0;
}
