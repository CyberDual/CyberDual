#include <lzma.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INPUT_SIZE 16384
#define COMPRESSED_SIZE (INPUT_SIZE + 4096)

int main(void)
{
	uint8_t input[INPUT_SIZE];
	uint8_t compressed[COMPRESSED_SIZE];
	uint8_t output[INPUT_SIZE];

	for (size_t i = 0; i < sizeof(input); ++i)
		input[i] = (uint8_t)((i % 256) * 2654435761u >> 16);

	lzma_options_lzma options;
	if (lzma_lzma_preset(&options, LZMA_PRESET_DEFAULT))
		return EXIT_FAILURE;
	options.dict_size = 4096;

	lzma_stream encoder = LZMA_STREAM_INIT;
	if (lzma_alone_encoder(&encoder, &options) != LZMA_OK)
		return EXIT_FAILURE;

	encoder.next_in = input;
	encoder.avail_in = sizeof(input);
	encoder.next_out = compressed;
	encoder.avail_out = sizeof(compressed);

	lzma_ret ret = lzma_code(&encoder, LZMA_FINISH);
	const size_t compressed_size = sizeof(compressed) - encoder.avail_out;
	lzma_end(&encoder);

	if (ret != LZMA_STREAM_END)
		return EXIT_FAILURE;

	lzma_stream decoder = LZMA_STREAM_INIT;
	if (lzma_alone_decoder(&decoder, UINT64_MAX) != LZMA_OK)
		return EXIT_FAILURE;

	decoder.next_in = compressed;
	decoder.avail_in = compressed_size;
	decoder.next_out = output;
	decoder.avail_out = sizeof(output);

	ret = lzma_code(&decoder, LZMA_FINISH);
	const size_t output_size = sizeof(output) - decoder.avail_out;
	lzma_end(&decoder);

	if (ret != LZMA_STREAM_END || output_size != sizeof(input)
			|| memcmp(input, output, sizeof(input)) != 0)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
