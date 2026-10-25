#include <lzma.h>
#include <stdint.h>
#include <stdio.h>

static void
put_vli(uint8_t **p, uint64_t v)
{
	while (v >= 0x80) {
		*(*p)++ = (uint8_t)((v & 0x7F) | 0x80);
		v >>= 7;
	}

	*(*p)++ = (uint8_t)v;
}

int
main(void)
{
	uint8_t buf[32];
	uint8_t *p = buf;

	/* Index Indicator */
	*p++ = 0x00;

	/* Number of Records: 2^32, which is within INDEX_RECORDS_MAX. */
	put_vli(&p, UINT64_C(1) << 32);

	/* One minimal Record so decoding proceeds into the list. */
	put_vli(&p, 5);
	put_vli(&p, 1);

	lzma_index *idx = NULL;
	lzma_stream strm = LZMA_STREAM_INIT;
	lzma_ret ret = lzma_index_decoder(&strm, &idx, UINT64_MAX);
	if (ret != LZMA_OK) {
		fprintf(stderr, "init %d\n", (int)ret);
		return 2;
	}

	strm.next_in = buf;
	strm.avail_in = (size_t)(p - buf);
	ret = lzma_code(&strm, LZMA_RUN);
	fprintf(stderr, "code %d\n", (int)ret);
	lzma_end(&strm);
	lzma_index_end(idx, NULL);
	return 0;
}
