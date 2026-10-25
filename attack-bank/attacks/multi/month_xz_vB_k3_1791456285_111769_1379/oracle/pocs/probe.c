// SPDX-License-Identifier: 0BSD
//
// Proof-of-concept for a reinit-after-allocation-failure memory-safety bug
// in liblzma's .lzma (LZMA_Alone) decoder, reached entirely through the
// public API (lzma_alone_decoder() + lzma_code() + a custom lzma_allocator).
//
// Normal usage: an application reuses one lzma_stream for several files and
// supplies its own allocator. If decoding a file needs a dictionary buffer
// that the allocator refuses (returns NULL), lzma_code() returns
// LZMA_MEM_ERROR. The application then reinitializes the same lzma_stream
// with lzma_alone_decoder() and decodes a smaller file. This must work.
//
// On a tree where the two reinit-safety fixes are both effective, every
// call returns a normal status code and the program exits 0. On a tree
// where they are jointly ineffective, the third decode reuses a dictionary
// buffer that was released (and set to NULL) during the failed allocation,
// which AddressSanitizer reports as an invalid write.

#include <lzma.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// The allocator refuses anything this large or larger. The 8 MiB
// dictionary request is far above this; the 4 KiB one is far below.
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

static void
my_free(void *opaque, void *ptr)
{
	(void)opaque;
	free(ptr);
}

static lzma_allocator my_allocator = { &my_alloc, &my_free, NULL };

// A valid .lzma stream with a 4 KiB dictionary and unknown size + EOPM.
// (tests/files/good-unknown_size-with_eopm.lzma)
static const uint8_t lzma_file[] = {
	0x5d, 0x00, 0x10, 0x00, 0x00, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x24, 0x19,
	0x49, 0x98, 0x6f, 0x05, 0x15, 0x27, 0x27, 0x0d,
	0x76, 0x78, 0xd0, 0x2a, 0x68, 0x17, 0x15, 0xff,
	0xff, 0x75, 0xf8, 0x00, 0x00
};

#define MEMLIMIT (64U << 20)

static lzma_ret
decode_once(lzma_stream *strm, const uint8_t *in, size_t in_size)
{
	uint8_t out[256];

	if (lzma_alone_decoder(strm, MEMLIMIT) != LZMA_OK) {
		fprintf(stderr, "alone_decoder init failed\n");
		exit(2);
	}

	strm->next_in = in;
	strm->avail_in = in_size;
	strm->next_out = out;
	strm->avail_out = sizeof(out);

	return lzma_code(strm, LZMA_FINISH);
}

int
main(void)
{
	// Small-dictionary stream (4 KiB): decodes successfully.
	uint8_t small[sizeof(lzma_file)];
	memcpy(small, lzma_file, sizeof(lzma_file));

	// Big-dictionary stream (8 MiB): same payload, larger dict size in
	// the header, so the decoder tries to allocate an 8 MiB dictionary.
	uint8_t big[sizeof(lzma_file)];
	memcpy(big, lzma_file, sizeof(lzma_file));
	big[2] = 0x00;
	big[3] = 0x80;	// dict size = 0x00800000 = 8 MiB

	lzma_stream strm = LZMA_STREAM_INIT;
	strm.allocator = &my_allocator;

	// 1) Decode the small stream: a 4 KiB dictionary buffer is allocated.
	lzma_ret r1 = decode_once(&strm, small, sizeof(small));
	printf("decode small #1: %d\n", (int)r1);

	// 2) Reinitialize and decode the big stream: the decoder releases the
	//    4 KiB dictionary and tries to allocate 8 MiB, which the allocator
	//    refuses -> LZMA_MEM_ERROR.
	lzma_ret r2 = decode_once(&strm, big, sizeof(big));
	printf("decode big   #2: %d\n", (int)r2);

	// 3) Reinitialize and decode the small stream again. A correct tree
	//    reallocates the dictionary and succeeds (or cleanly errors). On
	//    the buggy tree the released dictionary buffer is reused.
	lzma_ret r3 = decode_once(&strm, small, sizeof(small));
	printf("decode small #3: %d\n", (int)r3);

	lzma_end(&strm);
	printf("done\n");
	return 0;
}
