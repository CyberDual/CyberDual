#include <lzma.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int
main(void)
{
	/* Two-byte buffer, decode starting at the Index Indicator.
	   The Number of Records field is missing; the call must fail
	   without reading past the buffer. */
	size_t in_size = 2;
	uint8_t *in = malloc(in_size);
	if (in == NULL)
		return 1;

	in[0] = 0x00;
	in[1] = 0x00;

	size_t in_pos = 1;
	uint64_t memlimit = UINT64_C(1) << 20;
	lzma_index *idx = NULL;

	lzma_ret ret = lzma_index_buffer_decode(&idx, &memlimit, NULL,
			in, &in_pos, in_size);

	printf("ret=%d pos=%zu\n", (int)ret, in_pos);
	lzma_index_end(idx, NULL);
	free(in);
	return 0;
}
