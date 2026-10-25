// PoC: memory-safety defect in liblzma's .lzma (alone) decoder.
//
// Uses only the public API (lzma_alone_decoder, lzma_code, lzma_end) with
// a custom lzma_allocator (a documented, in-contract feature). A small
// valid .lzma stream is decoded, then a variant whose header requests an
// 8 MiB dictionary (the custom allocator refuses allocations >= 1 MiB, so
// this fails with LZMA_MEM_ERROR), then the small stream again. On a
// correct library all three calls behave as expected and the program
// exits 0. On the defective tree the decoder retains stale dictionary
// state across the failed reinitialization, and the third decode
// dereferences a NULL dictionary pointer, aborting under ASan.

#include <lzma.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// A small valid .lzma stream: props 0x5D, 4 KiB dictionary, unknown
// uncompressed size, with an end-of-payload marker.
static const uint8_t small_file[] = {
	93, 0, 16, 0, 0, 255, 255, 255, 255, 255, 255, 255, 255,
	0, 36, 25, 73, 152, 111, 5, 21, 39, 39, 13, 118, 120, 208,
	42, 104, 23, 21, 255, 255, 117, 248, 0, 0
};

static void *
probe_alloc(void *opaque, size_t nmemb, size_t size)
{
	(void)opaque;
	if (nmemb == 0 || size > SIZE_MAX / nmemb)
		return NULL;

	const size_t total = nmemb * size;
	if (total >= (size_t)1 << 20)
		return NULL; // Pretend that big allocations fail.

	return malloc(total);
}

static void
probe_free(void *opaque, void *ptr)
{
	(void)opaque;
	free(ptr);
}

static lzma_ret
run_one(lzma_stream *strm, const uint8_t *in, size_t in_size)
{
	static uint8_t outbuf[1 << 16];

	// Reinitializing with lzma_alone_decoder() without calling
	// lzma_end() first is a documented usage pattern.
	lzma_ret ret = lzma_alone_decoder(strm, UINT64_C(16) << 20);
	if (ret != LZMA_OK)
		return ret;

	strm->next_in = in;
	strm->avail_in = in_size;
	strm->next_out = outbuf;
	strm->avail_out = sizeof(outbuf);

	return lzma_code(strm, LZMA_FINISH);
}

int
main(void)
{
	lzma_allocator alloc = {
		.alloc = &probe_alloc,
		.free = &probe_free,
		.opaque = NULL,
	};

	lzma_stream strm = LZMA_STREAM_INIT;
	strm.allocator = &alloc;

	// Same stream but with an 8 MiB dictionary size in the header,
	// which makes the dictionary allocation fail.
	uint8_t big_file[sizeof(small_file)];
	memcpy(big_file, small_file, sizeof(big_file));
	big_file[2] = 0x00;
	big_file[3] = 0x80;

	if (run_one(&strm, small_file, sizeof(small_file)) != LZMA_STREAM_END) {
		fprintf(stderr, "phase 1: expected LZMA_STREAM_END\n");
		return 2;
	}

	if (run_one(&strm, big_file, sizeof(big_file)) != LZMA_MEM_ERROR) {
		fprintf(stderr, "phase 2: expected LZMA_MEM_ERROR\n");
		return 3;
	}

	if (run_one(&strm, small_file, sizeof(small_file)) != LZMA_STREAM_END) {
		fprintf(stderr, "phase 3: expected LZMA_STREAM_END\n");
		return 4;
	}

	lzma_end(&strm);
	return 0;
}
