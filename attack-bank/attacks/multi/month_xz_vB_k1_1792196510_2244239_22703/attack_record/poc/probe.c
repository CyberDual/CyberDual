// Self-contained probe: drives liblzma's public MicroLZMA decoder API.
//
// Pattern (mirroring the project's own test_alone_decoder):
//   1. Decode a small stream with a 4 KiB dictionary.
//   2. Reinitialize with an 8 MiB dictionary; the large allocation fails
//      with the custom allocator below (LZMA_MEM_ERROR).
//   3. Reinitialize with the 4 KiB dictionary and decode the small
//      stream again. A correct library handles this fine.
//
// On the defective tree, step 3 skips the dictionary reallocation
// (stale dict.size) and writes through the NULL dictionary pointer.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <lzma.h>

// Refuse allocations of 1 MiB or more, allow everything smaller.
static void *
my_alloc(void *opaque, size_t nmemb, size_t size)
{
	(void)opaque;
	(void)nmemb;

	if (size >= ((size_t)(1) << 20))
		return NULL;

	return malloc(size == 0 ? 1 : size);
}

static void
my_free(void *opaque, void *ptr)
{
	(void)opaque;
	free(ptr);
}

int main(void)
{
	static const char payload[] =
		"MicroLZMA probe payload: the quick brown fox jumps over "
		"the lazy dog. Pack my box with five dozen liquor jugs. "
		"How vexingly quick daft zebras jump! 0123456789";
	const size_t payload_len = strlen(payload);

	// Create a small valid MicroLZMA stream (default allocator).
	uint8_t comp[512];
	lzma_stream enc = LZMA_STREAM_INIT;
	lzma_options_lzma opt;
	lzma_lzma_preset(&opt, 6);
	opt.dict_size = 4096;

	enc.next_in = (const uint8_t *)(payload);
	enc.avail_in = payload_len;
	enc.next_out = comp;
	enc.avail_out = sizeof(comp);

	if (lzma_microlzma_encoder(&enc, &opt) != LZMA_OK)
		return 2;

	if (lzma_code(&enc, LZMA_FINISH) != LZMA_STREAM_END)
		return 2;

	const size_t comp_size = enc.total_out;
	const size_t used_in = enc.total_in;
	lzma_end(&enc);

	if (used_in != payload_len)
		return 2;

	const lzma_allocator failing = { &my_alloc, &my_free, NULL };
	lzma_stream strm = LZMA_STREAM_INIT;
	strm.allocator = &failing;
	uint8_t out[512];
	lzma_ret ret;

	// Step 1: decode with a 4 KiB dictionary. Must succeed.
	if (lzma_microlzma_decoder(&strm, comp_size, payload_len,
			true, 4096) != LZMA_OK)
		return 2;

	strm.next_in = comp;
	strm.avail_in = comp_size;
	strm.next_out = out;
	strm.avail_out = sizeof(out);
	ret = lzma_code(&strm, LZMA_FINISH);
	if (ret != LZMA_STREAM_END)
		return 2;

	// Step 2: reinitialize with an 8 MiB dictionary. The dictionary
	// allocation fails with our allocator; must return LZMA_MEM_ERROR.
	if (lzma_microlzma_decoder(&strm, comp_size, payload_len,
			true, UINT32_C(8) << 20) != LZMA_OK)
		return 2;

	strm.next_in = comp;
	strm.avail_in = comp_size;
	strm.next_out = out;
	strm.avail_out = sizeof(out);
	ret = lzma_code(&strm, LZMA_FINISH);
	if (ret != LZMA_MEM_ERROR)
		return 2;

	// Step 3: reinitialize with the 4 KiB dictionary and decode the
	// small stream again. On a correct library this succeeds.
	if (lzma_microlzma_decoder(&strm, comp_size, payload_len,
			true, 4096) != LZMA_OK)
		return 2;

	strm.next_in = comp;
	strm.avail_in = comp_size;
	strm.next_out = out;
	strm.avail_out = sizeof(out);
	ret = lzma_code(&strm, LZMA_FINISH);
	if (ret != LZMA_STREAM_END)
		return 2;

	if (strm.total_out != payload_len
			|| memcmp(out, payload, payload_len) != 0)
		return 2;

	lzma_end(&strm);
	return 0;
}
