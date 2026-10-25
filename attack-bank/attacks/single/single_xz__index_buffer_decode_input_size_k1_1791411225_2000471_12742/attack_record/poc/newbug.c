#include <lzma.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(void)
{
	// Build a minimal valid (empty) Index using the public encoder API.
	lzma_index *enc_idx = lzma_index_init(NULL);
	if (enc_idx == NULL)
		return 1;

	uint8_t real[64];
	size_t real_pos = 0;
	if (lzma_index_buffer_encode(enc_idx, real, &real_pos, sizeof(real))
			!= LZMA_OK)
		return 1;
	lzma_index_end(enc_idx, NULL);

	const size_t full_len = real_pos;
	if (full_len < 1)
		return 1;

	// Truncate the last byte (part of the Index CRC32) so that the
	// Index decoder legitimately runs out of real input while still
	// expecting more.
	const size_t truncated_len = full_len - 1;

	// Place the truncated Index at a non-zero offset inside a buffer
	// that is allocated to be exactly big enough to hold the prefix
	// plus the truncated Index -- no slack after it.
	const size_t prefix_len = 8;
	const size_t buf_size = prefix_len + truncated_len;

	uint8_t *buf = malloc(buf_size);
	if (buf == NULL)
		return 1;

	memset(buf, 0xAA, prefix_len);
	memcpy(buf + prefix_len, real, truncated_len);

	lzma_index *idx = NULL;
	uint64_t memlimit = UINT64_MAX;
	size_t in_pos = prefix_len;

	lzma_ret ret = lzma_index_buffer_decode(&idx, &memlimit, NULL,
			buf, &in_pos, buf_size);

	printf("ret = %d, in_pos = %zu\n", (int)ret, in_pos);

	if (idx != NULL)
		lzma_index_end(idx, NULL);
	free(buf);

	return 0;
}
