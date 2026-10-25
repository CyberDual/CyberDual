#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lzma.h"

static const uint8_t input[] = {
	0x5d, 0x00, 0x10, 0x00, 0x00, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x31, 0x19,
	0x4a, 0x20, 0xdc, 0x95, 0x22, 0x10, 0xdc, 0x67,
	0x7c, 0x94, 0xa8, 0x5b, 0xc7, 0x63, 0xb2, 0x3b,
	0x11, 0xe4, 0x67, 0xfc, 0x1a, 0xca, 0x7d, 0x22,
	0x09, 0xb1, 0x0c, 0x96, 0xb3, 0x7a, 0x85, 0x56,
	0xea, 0x98, 0x0f, 0xac, 0xb1, 0x51, 0x84, 0xdc,
	0x49, 0x60, 0x1b, 0xf3, 0xcb, 0xdc, 0x91, 0x5e,
	0x0e, 0x55, 0xc6, 0xbc, 0x35, 0x99, 0x6b, 0xc5,
	0xd4, 0x35, 0x7f, 0xff, 0xfe, 0xfe, 0x88, 0x00
};

static void *alloc(void *opaque, size_t nmemb, size_t size)
{
	(void)opaque;
	(void)nmemb;

	if (size > UINT32_C(1) << 20)
		return NULL;

	return malloc(size);
}

static void free_mem(void *opaque, void *ptr)
{
	(void)opaque;
	free(ptr);
}

static lzma_ret decode(lzma_stream *strm, const uint8_t *data,
		uint8_t *output)
{
	strm->next_in = data;
	strm->avail_in = sizeof(input);
	strm->next_out = output;
	strm->avail_out = sizeof(output);
	return lzma_code(strm, LZMA_FINISH);
}

int main(void)
{
	uint8_t large_input[sizeof(input)];
	uint8_t output[16384];
	lzma_allocator allocator = {
		.alloc = &alloc,
		.free = &free_mem,
	};
	lzma_stream strm = LZMA_STREAM_INIT;

	memcpy(large_input, input, sizeof(input));
	large_input[1] = 0x00;
	large_input[2] = 0x00;
	large_input[3] = 0x10;
	large_input[4] = 0x00;

	strm.allocator = &allocator;
	if (lzma_alone_decoder(&strm, UINT64_MAX) != LZMA_OK)
		return 1;
	if (decode(&strm, input, output) != LZMA_OK)
		return 2;

	if (lzma_alone_decoder(&strm, UINT64_MAX) != LZMA_OK)
		return 3;
	if (decode(&strm, large_input, output) != LZMA_MEM_ERROR)
		return 4;

	if (lzma_alone_decoder(&strm, UINT64_MAX) != LZMA_OK)
		return 5;
	if (decode(&strm, input, output) != LZMA_OK)
		return 6;

	lzma_end(&strm);
	return 0;
}
