// SPDX-License-Identifier: 0BSD
//
// Probe for the code-review benchmark. This is a normal liblzma
// application that uses only the public API.
//
// It decodes a few small .lzma (LZMA_Alone) files with a custom
// lzma_allocator that refuses very large allocations, reusing a single
// lzma_stream for several files. Reusing a decoder after an allocation
// failure is a usage pattern that liblzma explicitly supports (its own
// test suite covers it), so this is ordinary in-contract API usage.

#include <lzma.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// A valid .lzma stream: properties 0x5D (lc=3, lp=0, pb=2), 4 KiB
// dictionary, unknown uncompressed size, payload "x" followed by the
// end-of-payload marker.
static const uint8_t small_file[] = {
	0x5d, 0x00, 0x10, 0x00, 0x00, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x3c, 0x41,
	0xfb, 0xff, 0xff, 0xff, 0xe0, 0x00, 0x00, 0x00
};


#define MEMLIMIT (16U << 20)
#define TOO_BIG_ALLOC (1U << 20)


static void *
my_alloc(void *opaque, size_t nmemb, size_t size)
{
	(void)opaque;
	(void)nmemb;

	if (size >= TOO_BIG_ALLOC)
		return NULL;

	return malloc(size);
}


static lzma_allocator my_allocator = { &my_alloc, NULL, NULL };


static void
decode(lzma_stream *strm, const uint8_t *data, size_t size,
		lzma_ret expected, const char *what)
{
	uint8_t out[256];

	lzma_ret ret = lzma_alone_decoder(strm, MEMLIMIT);
	if (ret != LZMA_OK) {
		fprintf(stderr, "lzma_alone_decoder() failed for %s: %d\n",
				what, (int)ret);
		exit(1);
	}

	strm->next_in = data;
	strm->avail_in = size;
	strm->next_out = out;
	strm->avail_out = sizeof(out);

	ret = lzma_code(strm, LZMA_FINISH);
	if (ret != expected) {
		fprintf(stderr, "decoding %s: got %d, expected %d\n",
				what, (int)ret, (int)expected);
		exit(1);
	}
}


extern int
main(void)
{
	// The same stream as small_file but with an 8 MiB dictionary
	// size in the header. The payload is still valid; the decoder
	// just needs a bigger dictionary allocation.
	uint8_t big_file[sizeof(small_file)];
	memcpy(big_file, small_file, sizeof(big_file));
	big_file[2] = 0x00;
	big_file[3] = 0x80;

	lzma_stream strm = LZMA_STREAM_INIT;
	strm.allocator = &my_allocator;

	// Decode a file with a small dictionary.
	decode(&strm, small_file, sizeof(small_file),
			LZMA_STREAM_END, "the small file");

	// Decode a file whose dictionary allocation will fail.
	decode(&strm, big_file, sizeof(big_file),
			LZMA_MEM_ERROR, "the big-dictionary file");

	// Decode the small file again. The decoder must recover
	// cleanly from the earlier allocation failure.
	decode(&strm, small_file, sizeof(small_file),
			LZMA_STREAM_END, "the small file again");

	lzma_end(&strm);

	printf("OK\n");
	return 0;
}
