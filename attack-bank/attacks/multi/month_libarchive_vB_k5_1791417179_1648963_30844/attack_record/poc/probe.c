#include <archive.h>
#include <archive_entry.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static char
uu_char(unsigned value)
{
	value = (value & 63U) + 32U;
	return value == 32U ? '`' : (char)value;
}

int
main(void)
{
	struct archive *a;
	struct archive_entry *entry;
	char *input;
	char output[4096];
	size_t used = 0;
	int i, j, r;

	input = malloc(100000);
	if (input == NULL)
		return 1;
	memcpy(input, "begin 644 data\n", 15);
	used = 15;
	for (i = 0; i < 1200; ++i) {
		input[used++] = 'M';
		for (j = 0; j < 45; j += 3) {
			unsigned a0 = 0x41, a1 = 0x41, a2 = 0x41;
			input[used++] = uu_char(a0 >> 2);
			input[used++] = uu_char((a0 << 4) | (a1 >> 4));
			input[used++] = uu_char((a1 << 2) | (a2 >> 6));
			input[used++] = uu_char(a2);
		}
		input[used++] = '\n';
	}
	memcpy(input + used, "`\nend\n", 6);
	used += 6;

	a = archive_read_new();
	if (a == NULL)
		return 2;
	if (archive_read_support_filter_uu(a) != ARCHIVE_OK ||
	    archive_read_support_format_raw(a) != ARCHIVE_OK ||
	    archive_read_open_memory(a, input, used) != ARCHIVE_OK)
		return 3;
	if (archive_read_next_header(a, &entry) != ARCHIVE_OK)
		return 4;
	while ((r = (int)archive_read_data(a, output, sizeof(output))) > 0)
		;
	archive_read_free(a);
	free(input);
	return r == 0 ? 0 : 5;
}
