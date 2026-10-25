#include <lzma.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/* One Record past a single Index group. Unpadded Size uses the
   smallest value the encoder accepts so the Index stays compact. */
#define RECORD_COUNT 513u
#define UNPADDED 5u

static int
fail(const char *what, lzma_ret ret)
{
	fprintf(stderr, "%s failed: %d\n", what, (int)ret);
	return 1;
}

int
main(void)
{
	lzma_index *built = lzma_index_init(NULL);
	if (built == NULL) {
		fprintf(stderr, "lzma_index_init failed\n");
		return 1;
	}

	for (uint32_t i = 0; i < RECORD_COUNT; ++i) {
		const lzma_ret ret = lzma_index_append(built, NULL,
				UNPADDED, 1);
		if (ret != LZMA_OK)
			return fail("lzma_index_append", ret);
	}

	const lzma_vli index_size = lzma_index_size(built);
	if (index_size == 0 || index_size > SIZE_MAX) {
		fprintf(stderr, "unexpected Index size\n");
		return 1;
	}

	uint8_t *buf = malloc((size_t)index_size);
	if (buf == NULL)
		return 1;

	size_t out_pos = 0;
	lzma_ret ret = lzma_index_buffer_encode(built, buf, &out_pos,
			(size_t)index_size);
	lzma_index_end(built, NULL);
	if (ret != LZMA_OK)
		return fail("lzma_index_buffer_encode", ret);

	lzma_stream strm = LZMA_STREAM_INIT;
	lzma_index *decoded = NULL;
	ret = lzma_index_decoder(&strm, &decoded, UINT64_MAX);
	if (ret != LZMA_OK)
		return fail("lzma_index_decoder", ret);

	strm.next_in = buf;
	strm.avail_in = out_pos;

	/* Decode the whole Index. Memory accounting stays at the
	   preallocation estimate after Records start coming in. */
	ret = lzma_code(&strm, LZMA_FINISH);
	const uint64_t memused = lzma_memusage(&strm);

	fprintf(stderr, "decode ret=%d memusage=%llu records=%u\n",
			(int)ret,
			(unsigned long long)memused,
			RECORD_COUNT);

	lzma_end(&strm);
	free(buf);
	return ret == LZMA_STREAM_END ? 0 : 1;
}
