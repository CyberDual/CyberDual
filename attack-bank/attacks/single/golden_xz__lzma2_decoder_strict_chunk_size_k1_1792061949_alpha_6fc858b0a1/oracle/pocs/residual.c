#include <lzma.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
put_crc32(uint8_t *buf, size_t len)
{
	const uint32_t c = lzma_crc32(buf, len, 0);
	buf[len] = (uint8_t)c;
	buf[len + 1] = (uint8_t)(c >> 8);
	buf[len + 2] = (uint8_t)(c >> 16);
	buf[len + 3] = (uint8_t)(c >> 24);
}

/* Crafted .xz Stream: one LZMA2 chunk whose compressed size is smaller
   than the range-decoder initialization, with no payload bytes after
   the chunk header. */
static size_t
build_input(uint8_t *raw)
{
	size_t n = 0;

	raw[n++] = 0xFD;
	raw[n++] = 0x37;
	raw[n++] = 0x7A;
	raw[n++] = 0x58;
	raw[n++] = 0x5A;
	raw[n++] = 0x00;
	raw[n++] = 0x00;
	raw[n++] = 0x00;
	put_crc32(raw + 6, 2);
	n = 12;

	const size_t bh = n;
	raw[n++] = 0x02;
	raw[n++] = 0x00;
	raw[n++] = 0x21;
	raw[n++] = 0x01;
	raw[n++] = 0x00;
	raw[n++] = 0x00;
	raw[n++] = 0x00;
	raw[n++] = 0x00;
	put_crc32(raw + bh, 8);
	n = bh + 12;

	raw[n++] = 0xE0;
	raw[n++] = 0x00;
	raw[n++] = 0x00;
	raw[n++] = 0x00;
	raw[n++] = 0x00;
	raw[n++] = 0x5D;
	return n;
}

int
main(void)
{
	uint8_t raw[64];
	const size_t n = build_input(raw);

	uint8_t *in = malloc(n);
	if (in == NULL)
		return 1;
	memcpy(in, raw, n);

	lzma_stream strm = LZMA_STREAM_INIT;
	lzma_ret ret = lzma_stream_decoder(&strm, UINT64_MAX, 0);
	if (ret != LZMA_OK) {
		free(in);
		return 1;
	}

	uint8_t out[64];
	strm.next_in = in;
	strm.avail_in = n;
	strm.next_out = out;
	strm.avail_out = sizeof(out);

	ret = lzma_code(&strm, LZMA_FINISH);
	fprintf(stderr, "lzma_code returned %d\n", (int)ret);

	lzma_end(&strm);
	free(in);
	return 0;
}
