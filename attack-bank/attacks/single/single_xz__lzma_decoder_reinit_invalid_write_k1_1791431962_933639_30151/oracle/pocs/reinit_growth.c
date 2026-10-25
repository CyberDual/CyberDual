#include <lzma.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int
make_stream(uint8_t *encoded, size_t *encoded_size,
		uint32_t dict_size, size_t plain_size)
{
	uint8_t plain[10000];
	memset(plain, 'A', plain_size);

	lzma_options_lzma options;
	if (lzma_lzma_preset(&options, 1))
		return 1;

	options.dict_size = dict_size;
	lzma_stream stream = LZMA_STREAM_INIT;
	if (lzma_alone_encoder(&stream, &options) != LZMA_OK)
		return 1;

	stream.next_in = plain;
	stream.avail_in = plain_size;
	stream.next_out = encoded;
	stream.avail_out = *encoded_size;
	const lzma_ret ret = lzma_code(&stream, LZMA_FINISH);
	*encoded_size = stream.total_out;
	lzma_end(&stream);
	if (ret != LZMA_STREAM_END)
		return 1;

	for (size_t i = 0; i < 4; ++i)
		encoded[i + 1] = (uint8_t)(dict_size >> (i * 8));

	return 0;
}

int
main(void)
{
	uint8_t small[1024];
	uint8_t large[1024];
	size_t small_size = sizeof(small);
	size_t large_size = sizeof(large);
	if (make_stream(small, &small_size, 4096, 64)
			|| make_stream(large, &large_size, 4608, 10000))
		return 1;

	lzma_stream stream = LZMA_STREAM_INIT;
	uint8_t output[10000];
	const uint8_t *inputs[2] = { small, large };
	const size_t sizes[2] = { small_size, large_size };
	for (size_t i = 0; i < 2; ++i) {
		if (lzma_alone_decoder(&stream, UINT64_MAX) != LZMA_OK)
			return 2;

		stream.next_in = inputs[i];
		stream.avail_in = sizes[i];
		stream.next_out = output;
		stream.avail_out = sizeof(output);
		const lzma_ret ret = lzma_code(&stream, LZMA_FINISH);
		if (ret != LZMA_STREAM_END)
			return 3;
	}

	lzma_end(&stream);
	return 0;
}
