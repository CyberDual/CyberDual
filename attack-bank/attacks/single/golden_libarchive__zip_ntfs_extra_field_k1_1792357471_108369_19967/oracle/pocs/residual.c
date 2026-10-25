#include <archive.h>
#include <archive_entry.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(void)
{
	static const unsigned char signature[] = { 'P', 'K', 3, 4 };
	struct archive *archive;
	struct archive_entry *entry;
	unsigned char *zip;
	int result;

	zip = malloc(67);
	if (zip == NULL)
		return (1);
	memset(zip, 0, 67);
	memcpy(zip, signature, sizeof(signature));
	zip[4] = 20;
	zip[26] = 1;
	zip[28] = 36;
	zip[30] = 'x';
	zip[31] = 0x0a;
	zip[33] = 32;
	zip[39] = 1;
	zip[41] = 24;
	zip[59] = 1;

	archive = archive_read_new();
	if (archive == NULL) {
		free(zip);
		return (1);
	}
	archive_read_support_format_zip(archive);
	result = archive_read_open_memory(archive, zip, 67);
	if (result == ARCHIVE_OK)
		result = archive_read_next_header(archive, &entry);
	printf("archive_read_next_header: %d\n", result);
	archive_read_free(archive);
	free(zip);
	return (0);
}
