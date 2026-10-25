#include <lzma.h>
#include <stdint.h>
#include <stdlib.h>

int
main(void)
{
	lzma_index *source = lzma_index_init(NULL);
	if (source == NULL)
		return 1;

	if (lzma_index_append(source, NULL, 12, 32) != LZMA_OK
			|| lzma_index_append(source, NULL, 16, 64) != LZMA_OK)
		return 2;

	const size_t size = (size_t)lzma_index_size(source);
	uint8_t *data = malloc(size);
	if (data == NULL)
		return 3;

	size_t pos = 0;
	if (lzma_index_buffer_encode(source, data, &pos, size) != LZMA_OK)
		return 4;
	lzma_index_end(source, NULL);

	lzma_stream stream = LZMA_STREAM_INIT;
	lzma_index *decoded = NULL;
	if (lzma_index_decoder(&stream, &decoded, UINT64_MAX) != LZMA_OK)
		return 5;

	stream.next_in = data;
	stream.avail_in = pos;
	if (lzma_code(&stream, LZMA_FINISH) != LZMA_STREAM_END)
		return 6;

	volatile uint64_t usage = lzma_memusage(&stream);
	lzma_index_end(decoded, NULL);
	lzma_end(&stream);
	free(data);
	return usage == 0;
}
