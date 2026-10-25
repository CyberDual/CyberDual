// SPDX-License-Identifier: 0BSD

#include <stdio.h>

#include "lzma.h"


#define PRESET_DICT_SIZE (64 * 1024)


int
main(void)
{
	static uint8_t preset_dict[PRESET_DICT_SIZE];
	for (size_t i = 0; i < PRESET_DICT_SIZE; ++i)
		preset_dict[i] = (uint8_t)i;

	lzma_options_lzma options = {
		.dict_size = 4096,
		.preset_dict = preset_dict,
		.preset_dict_size = PRESET_DICT_SIZE,
		.lc = 3,
		.lp = 0,
		.pb = 2,
	};

	lzma_filter filters[2] = {
		{ LZMA_FILTER_LZMA1, &options },
		{ LZMA_VLI_UNKNOWN, NULL },
	};

	lzma_stream strm = LZMA_STREAM_INIT;
	const lzma_ret ret = lzma_raw_decoder(&strm, filters);

	printf("lzma_raw_decoder() returned %d\n", ret);

	if (ret == LZMA_OK)
		lzma_end(&strm);

	return ret == LZMA_OK ? 0 : 1;
}
