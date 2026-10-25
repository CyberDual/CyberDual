#include <lzma.h>
#include <stdio.h>
#include <stdlib.h>

static void *
lim_alloc(void *opaque, size_t nmemb, size_t size)
{
	(void)opaque;
	(void)nmemb;
	if (size > ((size_t)64 << 20))
		return NULL;
	return malloc(size);
}

static void
lim_free(void *opaque, void *ptr)
{
	(void)opaque;
	free(ptr);
}

int
main(void)
{
	/* lc/lp/pb = 3/0/2, dictionary size 0xFFFFFE00, size unknown. */
	static const uint8_t in[13] = {
		0x5D,
		0x00, 0xFE, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
	};
	uint8_t out[32];
	const lzma_allocator alloc = { lim_alloc, lim_free, NULL };
	lzma_stream strm = LZMA_STREAM_INIT;

	strm.allocator = &alloc;
	if (lzma_alone_decoder(&strm, UINT64_MAX) != LZMA_OK)
		return 2;

	strm.next_in = in;
	strm.avail_in = sizeof(in);
	strm.next_out = out;
	strm.avail_out = sizeof(out);

	const lzma_ret ret = lzma_code(&strm, LZMA_RUN);
	lzma_end(&strm);
	return ret == LZMA_OK || ret == LZMA_MEM_ERROR ? 0 : 1;
}
