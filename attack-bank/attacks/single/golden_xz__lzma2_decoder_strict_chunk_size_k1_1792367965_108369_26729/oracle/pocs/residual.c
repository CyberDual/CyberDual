#include <lzma.h>
#include <stdint.h>
#include <stdlib.h>

int
main(void)
{
	lzma_options_lzma options;
	if (lzma_lzma_preset(&options, 0))
		return 1;

	const size_t preset_size = options.dict_size;
	uint8_t *preset_dict = malloc(preset_size);
	if (preset_dict == NULL)
		return 2;

	for (size_t i = 0; i < preset_size; ++i)
		preset_dict[i] = (uint8_t)(i * 37U + (i >> 9));

	options.preset_dict = preset_dict;
	options.preset_dict_size = preset_size;

	lzma_filter filters[] = {
		{ .id = LZMA_FILTER_LZMA2, .options = &options },
		{ .id = LZMA_VLI_UNKNOWN, .options = NULL },
	};

	lzma_stream decoder = LZMA_STREAM_INIT;
	const lzma_ret ret = lzma_raw_decoder(&decoder, filters);

	lzma_end(&decoder);
	free(preset_dict);
	return ret == LZMA_OK ? 0 : 3;
}
