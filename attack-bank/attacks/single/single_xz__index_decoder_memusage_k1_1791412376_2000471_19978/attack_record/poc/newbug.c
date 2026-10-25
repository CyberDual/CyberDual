// Reproducer: lzma_memusage() dereferences a NULL lzma_index after the
// Index decoder has already finished (LZMA_STREAM_END), through the public
// liblzma streaming API.
//
// Steps:
//   1. Build a small lzma_index with a couple of Records and encode it
//      to a buffer with lzma_index_buffer_encode() (public API).
//   2. Initialize a streaming Index decoder with lzma_index_decoder() and
//      feed it the encoded buffer via lzma_code() until LZMA_STREAM_END.
//   3. Call lzma_memusage(strm), which an application may reasonably do
//      to inspect memory usage/statistics after decoding completes and
//      before calling lzma_end().

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lzma.h>

int
main(void)
{
	lzma_index *idx = lzma_index_init(NULL);
	if (idx == NULL)
		return 1;

	if (lzma_index_append(idx, NULL, 64, 1000) != LZMA_OK
			|| lzma_index_append(idx, NULL, 64, 2000) != LZMA_OK) {
		fprintf(stderr, "lzma_index_append failed\n");
		return 1;
	}

	uint8_t buf[256];
	size_t out_pos = 0;
	if (lzma_index_buffer_encode(idx, buf, &out_pos, sizeof(buf))
			!= LZMA_OK) {
		fprintf(stderr, "lzma_index_buffer_encode failed\n");
		return 1;
	}

	lzma_index_end(idx, NULL);

	lzma_stream strm = LZMA_STREAM_INIT;
	lzma_index *decoded = NULL;
	if (lzma_index_decoder(&strm, &decoded, UINT64_MAX) != LZMA_OK) {
		fprintf(stderr, "lzma_index_decoder failed\n");
		return 1;
	}

	strm.next_in = buf;
	strm.avail_in = out_pos;

	lzma_ret ret;
	do {
		ret = lzma_code(&strm, LZMA_RUN);
	} while (ret == LZMA_OK && strm.avail_in > 0);

	if (ret != LZMA_STREAM_END) {
		fprintf(stderr, "Index decoding did not finish: %d\n", ret);
		return 1;
	}

	// Decoding is done; coder->index was handed off to "decoded" and
	// set to NULL internally. Querying memory usage now is a normal,
	// documented operation (it works at any point while strm is a
	// live, non-ended Index-decoder stream) and should not crash.
	uint64_t memusage = lzma_memusage(&strm);
	printf("memusage = %llu\n", (unsigned long long)memusage);

	lzma_end(&strm);
	lzma_index_end(decoded, NULL);

	return 0;
}
