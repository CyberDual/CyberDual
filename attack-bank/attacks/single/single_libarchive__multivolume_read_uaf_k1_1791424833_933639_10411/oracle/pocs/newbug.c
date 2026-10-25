#include <archive.h>
#include <archive_entry.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int
main(void)
{
	char dir[] = "/tmp/libarchive-vol-XXXXXX";
	char first[128], second[128];
	const char *names[3];
	char *archive_data, payload[131072], output[8192];
	size_t archive_size = 0;
	struct archive *writer, *reader;
	struct archive_entry *entry;
	pid_t child;
	int fd, status;

	if (mkdtemp(dir) == NULL)
		return 1;
	snprintf(first, sizeof(first), "%s/first", dir);
	snprintf(second, sizeof(second), "%s/second", dir);
	archive_data = malloc(262144);
	if (archive_data == NULL)
		return 1;
	memset(payload, 'A', sizeof(payload));
	writer = archive_write_new();
	archive_write_set_format_ustar(writer);
	if (archive_write_open_memory(writer, archive_data, 262144,
	    &archive_size) != ARCHIVE_OK)
		return 1;
	entry = archive_entry_new();
	archive_entry_set_pathname(entry, "file");
	archive_entry_set_filetype(entry, AE_IFREG);
	archive_entry_set_perm(entry, 0644);
	archive_entry_set_size(entry, sizeof(payload));
	if (archive_write_header(writer, entry) != ARCHIVE_OK ||
	    archive_write_data(writer, payload, sizeof(payload)) !=
	    (ssize_t)sizeof(payload))
		return 1;
	archive_entry_free(entry);
	archive_write_free(writer);
	if (mkfifo(first, 0600) != 0)
		return 1;
	fd = open(second, O_CREAT | O_WRONLY | O_TRUNC, 0600);
	if (fd < 0 || write(fd, archive_data + 1024, archive_size - 1024) !=
	    (ssize_t)(archive_size - 1024))
		return 1;
	close(fd);
	child = fork();
	if (child < 0)
		return 1;
	if (child == 0) {
		fd = open(first, O_WRONLY);
		if (fd < 0 || write(fd, archive_data, 1024) != 1024)
			_exit(1);
		close(fd);
		_exit(0);
	}
	names[0] = first;
	names[1] = second;
	names[2] = NULL;
	reader = archive_read_new();
	archive_read_support_format_tar(reader);
	if (archive_read_open_filenames(reader, names, 1024) != ARCHIVE_OK ||
	    archive_read_next_header(reader, &entry) != ARCHIVE_OK)
		return 1;
	while (archive_read_data(reader, output, sizeof(output)) > 0)
		;
	archive_read_free(reader);
	waitpid(child, &status, 0);
	unlink(first);
	unlink(second);
	rmdir(dir);
	free(archive_data);
	return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : 1;
}
