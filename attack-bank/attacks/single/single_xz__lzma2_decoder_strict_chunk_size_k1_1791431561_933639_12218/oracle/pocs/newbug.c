#include <lzma.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int
main(void)
{
	static const uint8_t prefix[] = {
		0xfd, 0x37, 0x7a, 0x58, 0x5a, 0x00, 0x00, 0x04,
		0xe6, 0xd6, 0xb4, 0x46, 0x02, 0x00, 0x21, 0x01,
		0x16, 0x00, 0x00, 0x00, 0x74, 0x2f, 0xe5, 0xa3,
		0xe0, 0x00, 0x09, 0x00, 0x00, 0x5d,
	};
	uint8_t *input = malloc(sizeof(prefix));
	if (input == NULL)
		return 2;

	memcpy(input, prefix, sizeof(prefix));
	uint8_t output[4096];
	lzma_stream strm = LZMA_STREAM_INIT;
	if (lzma_stream_decoder(&strm, UINT64_MAX, 0) != LZMA_OK) {
		free(input);
		return 2;
	}

	strm.next_in = input;
	strm.avail_in = sizeof(prefix);
	strm.next_out = output;
	strm.avail_out = sizeof(output);
	const lzma_ret ret = lzma_code(&strm, LZMA_RUN);
	lzma_end(&strm);
	free(input);
	return ret == LZMA_OK ? 0 : 1;
}
