#include <lzma.h>

#include <stdint.h>
#include <stdio.h>

/* One .xz Index with more Records than fit in a single Record group. */
enum { RECORD_COUNT = 513 };

static void
put_vli(uint8_t **p, uint64_t n)
{
	while (n >= 0x80) {
		*(*p)++ = (uint8_t)((n & 0x7F) | 0x80);
		n >>= 7;
	}

	*(*p)++ = (uint8_t)n;
}

int
main(void)
{
	uint8_t buf[16 + RECORD_COUNT * 2];
	uint8_t *p = buf;

	*p++ = 0x00;
	put_vli(&p, RECORD_COUNT);

	for (unsigned i = 0; i < RECORD_COUNT; ++i) {
		put_vli(&p, 5);
		put_vli(&p, 0);
	}

	while ((size_t)(p - buf) % 4 != 0)
		*p++ = 0x00;

	const size_t body = (size_t)(p - buf);
	const uint32_t crc = lzma_crc32(buf, body, 0);
	*p++ = (uint8_t)crc;
	*p++ = (uint8_t)(crc >> 8);
	*p++ = (uint8_t)(crc >> 16);
	*p++ = (uint8_t)(crc >> 24);

	lzma_index *idx = NULL;
	uint64_t memlimit = UINT64_C(1) << 30;
	size_t in_pos = 0;
	const lzma_ret ret = lzma_index_buffer_decode(&idx, &memlimit, NULL,
			buf, &in_pos, (size_t)(p - buf));
	if (ret != LZMA_OK) {
		fprintf(stderr, "lzma_index_buffer_decode returned %d\n",
				(int)ret);
		return 1;
	}

	lzma_index_end(idx, NULL);
	return 0;
}
