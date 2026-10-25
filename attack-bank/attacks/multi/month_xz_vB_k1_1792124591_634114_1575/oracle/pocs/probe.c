#include <lzma.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

int main(void)
{
	enum { N = 4096 };
	uint8_t *plain = malloc(N);
	if (plain == NULL)
		return 1;
	for (size_t i = 0; i < N; ++i)
		plain[i] = (uint8_t)(i * 17u + 3u);

	lzma_options_lzma opt;
	if (lzma_lzma_preset(&opt, 1))
		return 1;
	opt.dict_size = 4096;

	lzma_filter filters[2] = {
		{ .id = LZMA_FILTER_LZMA2, .options = &opt },
		{ .id = LZMA_VLI_UNKNOWN, .options = NULL },
	};

	size_t comp_cap = (size_t)N + 8192;
	uint8_t *comp = malloc(comp_cap);
	uint8_t *out = malloc(N);
	if (comp == NULL || out == NULL)
		return 1;

	size_t comp_pos = 0;
	if (lzma_raw_buffer_encode(filters, NULL, plain, N,
			comp, &comp_pos, comp_cap) != LZMA_OK)
		return 1;

	size_t in_pos = 0;
	size_t out_pos = 0;
	lzma_ret dr = lzma_raw_buffer_decode(filters, NULL,
			comp, &in_pos, comp_pos, out, &out_pos, N);
	int bad = !(dr == LZMA_OK && out_pos == N
			&& memcmp(out, plain, N) == 0);
	free(plain);
	free(comp);
	free(out);
	return bad;
}
