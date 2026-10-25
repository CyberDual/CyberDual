// PoC: lzma_memusage() on a finished Index decoder stream.
//
// Builds a small valid Index with the public API, decodes it with the
// streaming Index decoder (lzma_index_decoder), and then queries the
// memory usage of the still-initialized stream with lzma_memusage(),
// which the API contract allows at any time after initialization.
//
// On a fixed liblzma this returns a small non-zero value and exits
// cleanly. On the defective tree the memusage query dereferences a
// NULL lzma_index pointer (the decoded Index has already been handed
// to the caller and the decoder's internal pointer cleared), which
// aborts under AddressSanitizer.

#include <stdio.h>
#include <stdint.h>
#include <lzma.h>

int
main(void)
{
	// Construct a small valid Index with two Records using the
	// public API only.
	lzma_index *idx = lzma_index_init(NULL);
	if (idx == NULL)
		return 1;

	if (lzma_index_append(idx, NULL, 5, 100) != LZMA_OK)
		return 1;
	if (lzma_index_append(idx, NULL, 12, 200) != LZMA_OK)
		return 1;

	// Encode the Index into a buffer.
	uint8_t buf[128];
	size_t buf_pos = 0;
	if (lzma_index_buffer_encode(idx, buf, &buf_pos, sizeof(buf))
			!= LZMA_OK)
		return 1;

	lzma_index_end(idx, NULL);

	// Decode the Index with the streaming Index decoder.
	lzma_stream strm = LZMA_STREAM_INIT;
	lzma_index *decoded = NULL;
	if (lzma_index_decoder(&strm, &decoded, UINT64_MAX) != LZMA_OK)
		return 1;

	strm.next_in = buf;
	strm.avail_in = buf_pos;
	if (lzma_code(&strm, LZMA_FINISH) != LZMA_STREAM_END)
		return 1;

	// The stream is still initialized, so querying its memory usage
	// is a documented, always-legal operation.
	const uint64_t mu = lzma_memusage(&strm);
	printf("memusage=%llu\n", (unsigned long long)mu);

	lzma_end(&strm);
	lzma_index_end(decoded, NULL);
	return 0;
}
