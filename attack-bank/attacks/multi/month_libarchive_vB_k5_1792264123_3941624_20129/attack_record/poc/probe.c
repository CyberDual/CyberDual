#include <archive.h>
#include <archive_entry.h>
#include <stdio.h>
#include <string.h>

static int
write_volume(const char *path, size_t size)
{
	unsigned char data[200] = { 0 };
	FILE *file;

	file = fopen(path, "wb");
	if (file == NULL)
		return (0);
	if (size != 0 && fwrite(data, 1, size, file) != size) {
		fclose(file);
		return (0);
	}
	return (fclose(file) == 0);
}

int
main(void)
{
	const char *files[] = {
		"/tmp/libarchive-probe-volume-1",
		"/tmp/libarchive-probe-volume-2",
		NULL
	};
	struct archive *archive;
	struct archive_entry *entry;
	int result = 0;

	if (!write_volume(files[0], sizeof(unsigned char) * 200) ||
	    !write_volume(files[1], 0))
		return (2);

	archive = archive_read_new();
	if (archive == NULL) {
		result = 2;
		goto cleanup_files;
	}
	if (archive_read_support_format_tar(archive) != ARCHIVE_OK ||
	    archive_read_support_format_cpio(archive) != ARCHIVE_OK) {
		archive_read_free(archive);
		result = 2;
		goto cleanup_files;
	}
	if (archive_read_open_filenames(archive, files, 4096) != ARCHIVE_OK) {
		archive_read_free(archive);
		goto cleanup_files;
	}
	while (archive_read_next_header(archive, &entry) == ARCHIVE_OK)
		;
	if (archive_read_free(archive) != ARCHIVE_OK)
		result = 2;

cleanup_files:
	remove(files[0]);
	remove(files[1]);
	return (result);
}
