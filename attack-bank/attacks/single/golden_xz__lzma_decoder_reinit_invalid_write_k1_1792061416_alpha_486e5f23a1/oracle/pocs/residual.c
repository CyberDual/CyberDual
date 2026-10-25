#include <lzma.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Reject dictionary-sized allocations while letting the small structures
   and a 4 KiB dictionary through. */
static void *
fail_large(void *opaque, size_t nmemb, size_t size)
{
	(void)opaque;
	if (nmemb != 0 && size > ((size_t)-1) / nmemb)
		return NULL;
	if (nmemb * size >= (1u << 20))
		return NULL;
	return malloc(nmemb * size);
}

static void
do_free(void *opaque, void *ptr)
{
	(void)opaque;
	free(ptr);
}

static const uint8_t small_lzma[] = {
	0x5d, 0x00, 0x10, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x34, 0x19,
	0x49, 0xee, 0x8d, 0xe9, 0x17, 0x89, 0x3a, 0x33, 0x5f, 0xfc, 0xb3, 0xec, 0xe5, 0xb0, 0xdc, 0x93,
	0xff, 0xff, 0xdf, 0x36, 0x00, 0x00,
};

static const uint8_t big_lzma[] = {
	0x5d, 0x00, 0x00, 0x80, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x20, 0xef,
	0xfb, 0xbf, 0xfe, 0xa3, 0xb1, 0x5e, 0xe5, 0xf8, 0x3f, 0xb2, 0xaa, 0x26, 0x55, 0xf8, 0x68, 0x70,
	0x41, 0x70, 0x15, 0x0f, 0x8d, 0xfd, 0x1e, 0x4c, 0x1b, 0x8a, 0x42, 0xb7, 0x19, 0xf4, 0x69, 0x18,
	0x71, 0xae, 0x66, 0x23, 0x8a, 0x8a, 0x4d, 0x2f, 0xa3, 0x0d, 0xd9, 0x7f, 0xa6, 0xe3, 0x8c, 0x23,
	0x11, 0x53, 0xdc, 0x7d, 0x22, 0x80, 0xed, 0xff, 0xff, 0xfb, 0x29, 0x20, 0x00,
};

static lzma_ret
decode_once(lzma_stream *strm, const uint8_t *in, size_t in_size,
		uint8_t *out, size_t out_size)
{
	lzma_ret ret;

	ret = lzma_alone_decoder(strm, UINT64_C(1) << 30);
	if (ret != LZMA_OK)
		return ret;

	strm->next_in = in;
	strm->avail_in = in_size;
	strm->next_out = out;
	strm->avail_out = out_size;

	ret = lzma_code(strm, LZMA_FINISH);
	return ret;
}

int
main(void)
{
	static const lzma_allocator allocator = {
		.alloc = fail_large,
		.free = do_free,
		.opaque = NULL,
	};
	lzma_stream strm = LZMA_STREAM_INIT;
	uint8_t out[64 * 1024];
	lzma_ret ret;

	strm.allocator = &allocator;

	ret = decode_once(&strm, small_lzma, sizeof(small_lzma),
			out, sizeof(out));
	if (ret != LZMA_STREAM_END) {
		fprintf(stderr, "small decode returned %d\n", (int)ret);
		return 1;
	}

	ret = decode_once(&strm, big_lzma, sizeof(big_lzma),
			out, sizeof(out));
	if (ret != LZMA_MEM_ERROR) {
		fprintf(stderr, "big decode returned %d, expected MEM_ERROR\n",
				(int)ret);
		return 1;
	}

	/* Same stream, same dictionary size as the allocation that failed. */
	ret = decode_once(&strm, big_lzma, sizeof(big_lzma),
			out, sizeof(out));
	fprintf(stderr, "retry returned %d\n", (int)ret);
	lzma_end(&strm);
	return 0;
}
