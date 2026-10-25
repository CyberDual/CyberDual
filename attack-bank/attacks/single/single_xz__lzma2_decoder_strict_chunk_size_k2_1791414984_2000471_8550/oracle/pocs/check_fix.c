// Sanity check (not part of the submitted PoCs) that an undersized
// compressed_size in an LZMA2 chunk header is now rejected with
// LZMA_DATA_ERROR instead of silently decoding into the next chunk.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lzma.h"

int
main(void)
{
	size_t text_size = 4096;
	uint8_t *text = malloc(text_size);
	for (size_t i = 0; i < text_size; ++i)
		text[i] = (uint8_t)(i * 2654435761u >> 24);

	lzma_options_lzma opt;
	lzma_lzma_preset(&opt, 9);

	lzma_filter filters[2] = {
		{ .id = LZMA_FILTER_LZMA2, .options = &opt },
		{ .id = LZMA_VLI_UNKNOWN, .options = NULL },
	};

	size_t enc_cap = text_size * 2 + 1024;
	uint8_t *enc = malloc(enc_cap);
	size_t enc_size = 0;
	if (lzma_raw_buffer_encode(filters, NULL, text, text_size, enc,
			&enc_size, enc_cap) != LZMA_OK) {
		fprintf(stderr, "encode failed\n");
		return 1;
	}

	size_t pos = 0;
	uint32_t control = enc[pos++];
	pos += 2; // uncompressed size low bytes
	uint32_t compressed_size = (uint32_t)enc[pos] << 8;
	pos += 1;
	compressed_size += enc[pos] + 1U;
	pos += 1;
	if (control >= 0xC0)
		pos += 1; // properties byte

	printf("header=%zu compressed_size=%u total=%zu\n",
			pos, compressed_size, enc_size);

	// Corrupt the compressed_size field to claim a chunk that is
	// smaller than what's actually there (undersized), e.g. half.
	uint32_t fake_size = compressed_size / 2;
	uint32_t encoded = fake_size - 1U;
	enc[pos - 2] = (uint8_t)(encoded >> 8);
	enc[pos - 1] = (uint8_t)encoded;

	lzma_stream strm = LZMA_STREAM_INIT;
	if (lzma_raw_decoder(&strm, filters) != LZMA_OK) {
		fprintf(stderr, "raw_decoder failed\n");
		return 1;
	}

	uint8_t *out = malloc(text_size + 4096);
	strm.next_in = enc;
	strm.avail_in = enc_size;
	strm.next_out = out;
	strm.avail_out = text_size + 4096;

	lzma_ret ret;
	do {
		ret = lzma_code(&strm, LZMA_FINISH);
	} while (ret == LZMA_OK);

	printf("final ret = %d (%s)\n", (int)ret,
			ret == LZMA_DATA_ERROR ? "DATA_ERROR as expected" :
			ret == LZMA_STREAM_END ? "STREAM_END (BUG: accepted bad chunk!)" :
			"other");

	lzma_end(&strm);
	return 0;
}
