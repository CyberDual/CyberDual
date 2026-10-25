// Test case for lzma_index_buffer_decode(): the application has
// already consumed the whole input buffer when the single-call
// Index decoder is invoked, so no input remains for the decoder.
// Per the API contract, only *in_pos > in_size is an error, so
// *in_pos == in_size must be handled gracefully (LZMA_DATA_ERROR).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lzma.h>

int
main(void)
{
	// 12-byte Stream Header followed by the smallest possible Index
	// (an empty Index: indicator, Number of Records = 0, two padding
	// bytes, and the CRC32 of those four bytes).
	const uint8_t data[] = {
		0xFD, 0x37, 0x7A, 0x58, 0x5A, 0x00, 0x00, 0x04,
		0xE6, 0xD6, 0xB4, 0x46,
		0x00, 0x00, 0x00, 0x00, 0x1C, 0xDF, 0x44, 0x21,
	};
	const size_t in_size = sizeof(data);

	uint8_t *in = malloc(in_size);
	if (in == NULL)
		return 77;
	memcpy(in, data, in_size);

	// The Stream Header and the Index were already parsed elsewhere,
	// so the decode position is at the very end of the buffer.
	size_t in_pos = in_size;
	uint64_t memlimit = UINT64_MAX;
	lzma_index *idx = NULL;

	const lzma_ret ret = lzma_index_buffer_decode(
			&idx, &memlimit, NULL, in, &in_pos, in_size);

	printf("lzma_index_buffer_decode: ret=%d\n", (int)ret);

	lzma_index_end(idx, NULL);
	free(in);

	// The empty remaining input cannot contain an Index.
	return ret == LZMA_DATA_ERROR ? EXIT_SUCCESS : EXIT_FAILURE;
}
