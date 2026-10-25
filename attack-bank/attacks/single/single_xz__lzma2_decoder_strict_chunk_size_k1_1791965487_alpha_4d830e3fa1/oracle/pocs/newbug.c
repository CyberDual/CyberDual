#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <lzma.h>

static void
put_le32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)v;
	p[1] = (uint8_t)(v >> 8);
	p[2] = (uint8_t)(v >> 16);
	p[3] = (uint8_t)(v >> 24);
}

/* .xz Stream, Check=None, one LZMA2 chunk, buffer ends on the chunk. */
static uint8_t *
build_stream(size_t *out_size)
{
	uint8_t flags[2] = { 0x00, 0x00 };
	uint8_t hdr[8] = {
		0x02, /* Block Header Size: (2 + 1) * 4 = 12 */
		0x00, /* one filter, sizes omitted */
		0x21, /* LZMA2 */
		0x01, /* properties size */
		0x00, /* dictionary size 4 KiB */
		0x00, 0x00, 0x00
	};
	/* Dict reset, uncompressed size 1, compressed size 1, lc/lp/pb,
	   and the single compressed byte (range decoder init). */
	uint8_t chunk[7] = {
		0xE0, 0x00, 0x00, 0x00, 0x00, 0x5D, 0x00
	};

	const size_t n = 6 + 2 + 4 + 8 + 4 + sizeof(chunk);
	uint8_t *buf = malloc(n);
	if (buf == NULL)
		return NULL;

	size_t p = 0;
	memcpy(buf + p, "\xFD\x37\x7A\x58\x5A\x00", 6);
	p += 6;
	memcpy(buf + p, flags, 2);
	p += 2;
	put_le32(buf + p, lzma_crc32(flags, 2, 0));
	p += 4;
	memcpy(buf + p, hdr, 8);
	p += 8;
	put_le32(buf + p, lzma_crc32(hdr, 8, 0));
	p += 4;
	memcpy(buf + p, chunk, sizeof(chunk));
	p += sizeof(chunk);

	*out_size = p;
	return buf;
}

int
main(void)
{
	size_t in_size = 0;
	uint8_t *in = build_stream(&in_size);
	if (in == NULL)
		return 1;

	uint64_t memlimit = 1U << 20;
	size_t in_pos = 0;
	uint8_t out[32];
	size_t out_pos = 0;

	lzma_ret ret = lzma_stream_buffer_decode(&memlimit, 0, NULL,
			in, &in_pos, in_size, out, &out_pos, sizeof(out));
	fprintf(stderr, "ret=%d in_pos=%zu out_pos=%zu\n",
			(int)ret, in_pos, out_pos);
	free(in);
	return 0;
}
