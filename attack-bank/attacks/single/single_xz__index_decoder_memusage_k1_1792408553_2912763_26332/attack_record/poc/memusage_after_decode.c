// PoC: lzma_memusage() queried on an lzma_index_decoder stream that has
// finished decoding an Index. Builds a valid .xz Index in memory, decodes
// it incrementally with lzma_code(), prints the memusage estimate while
// Records are being decoded, and queries the estimate once more after
// LZMA_STREAM_END.
//
// Expected: memusage grows (never shrinks) while Records are decoded and
// the final query after successful decoding prints a small value.

#include <lzma.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RECORDS 600

// Encode a value as an xz variable-length integer (LEB128).
static size_t
put_vli(uint8_t *out, uint64_t v)
{
	size_t n = 0;
	do {
		uint8_t b = v & 0x7F;
		v >>= 7;
		if (v != 0)
			b |= 0x80;
		out[n++] = b;
	} while (v != 0);
	return n;
}

int
main(void)
{
	setvbuf(stdout, NULL, _IOLBF, 0);

	// Build a valid Index: Indicator, Number of Records, the Records
	// (Unpadded Size = 5, Uncompressed Size = 1), Padding, CRC32.
	static uint8_t buf[4096];
	size_t size = 0;

	buf[size++] = 0x00; // Index Indicator
	size += put_vli(buf + size, RECORDS);

	for (int i = 0; i < RECORDS; i++) {
		size += put_vli(buf + size, 5); // Unpadded Size
		size += put_vli(buf + size, 1); // Uncompressed Size
	}

	while ((size & 3) != 0)
		buf[size++] = 0x00; // Index Padding

	const uint32_t crc = lzma_crc32(buf, size, 0);
	for (int i = 0; i < 4; i++)
		buf[size++] = (uint8_t)(crc >> (i * 8));

	// Initialize the Index decoder.
	lzma_stream strm = LZMA_STREAM_INIT;
	lzma_index *idx = NULL;
	if (lzma_index_decoder(&strm, &idx, UINT64_MAX) != LZMA_OK) {
		fprintf(stderr, "lzma_index_decoder failed\n");
		return 1;
	}

	// Decode the Index a few bytes at a time, tracking the memusage
	// estimate as the Records are being decoded.
	uint8_t out[8];
	size_t pos = 0;
	lzma_ret ret = LZMA_OK;
	uint64_t prev_mu = 0;
	strm.next_in = buf;
	strm.avail_in = 0;

	while (ret == LZMA_OK) {
		if (strm.avail_in == 0 && pos < size) {
			size_t chunk = size - pos;
			if (chunk > 7)
				chunk = 7;
			strm.next_in = buf + pos;
			strm.avail_in = chunk;
			pos += chunk;
		}

		strm.next_out = out;
		strm.avail_out = sizeof(out);

		ret = lzma_code(&strm, pos == size ? LZMA_FINISH : LZMA_RUN);

		const uint64_t mu = lzma_memusage(&strm);
		if (mu < prev_mu)
			printf("pos=%zu: memusage SHRANK: %" PRIu64
					" -> %" PRIu64 "\n",
					pos, prev_mu, mu);
		else if (mu != prev_mu)
			printf("pos=%zu: memusage grew: %" PRIu64
					" -> %" PRIu64 "\n",
					pos, prev_mu, mu);
		prev_mu = mu;
	}

	if (ret != LZMA_STREAM_END) {
		fprintf(stderr, "lzma_code failed: %d\n", ret);
		return 1;
	}

	printf("decoded %u Records; memusage while decoding ended at %"
			PRIu64 "\n", (unsigned)lzma_index_block_count(idx),
			prev_mu);

	// Query the estimate once more now that decoding is done.
	printf("memusage after decode: %" PRIu64 "\n",
			lzma_memusage(&strm));

	lzma_index_end(idx, NULL);
	lzma_end(&strm);
	return 0;
}
