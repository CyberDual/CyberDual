#include <archive.h>
#include <archive_entry.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

/*
 * Multi-volume read where the first volume is a non-disk-like client
 * (a pipe, so no block-size rounding) and the second volume is a
 * regular file large enough that file_open() rounds its block size
 * up to 64KiB.
 */
int
main(void)
{
	const char *filenames[3];
	char volume2_path[] = "/tmp/newbug_volume2.XXXXXX";
	int fd2;
	int pipefds[2];
	pid_t pid;
	struct archive *a;
	struct archive_entry *ae;
	size_t volume2_size = 200000;
	char *buf;
	ssize_t written;
	int r;

	fd2 = mkstemp(volume2_path);
	if (fd2 < 0) {
		perror("mkstemp");
		return 1;
	}
	unlink(volume2_path);

	buf = malloc(volume2_size);
	memset(buf, 'B', volume2_size);
	written = write(fd2, buf, volume2_size);
	if (written != (ssize_t)volume2_size) {
		perror("write volume2");
		return 1;
	}
	free(buf);
	lseek(fd2, 0, SEEK_SET);

	/* Make volume2 accessible by path through /proc/self/fd. */
	char volume2_fdpath[64];
	snprintf(volume2_fdpath, sizeof(volume2_fdpath), "/proc/self/fd/%d", fd2);

	if (pipe(pipefds) != 0) {
		perror("pipe");
		return 1;
	}

	pid = fork();
	if (pid < 0) {
		perror("fork");
		return 1;
	}
	if (pid == 0) {
		/* Child: feed volume1 bytes down the pipe, then exit. */
		close(pipefds[0]);
		char v1[64];
		memset(v1, 'A', sizeof(v1));
		write(pipefds[1], v1, sizeof(v1));
		close(pipefds[1]);
		_exit(0);
	}
	close(pipefds[1]);

	/* Redirect stdin to the pipe's read end so volume1 is "" (stdin). */
	dup2(pipefds[0], 0);
	close(pipefds[0]);

	filenames[0] = "";
	filenames[1] = volume2_fdpath;
	filenames[2] = NULL;

	a = archive_read_new();
	archive_read_support_filter_none(a);
	archive_read_support_format_raw(a);

	r = archive_read_open_filenames(a, filenames, 512);
	if (r != ARCHIVE_OK) {
		fprintf(stderr, "open failed: %s\n", archive_error_string(a));
		return 1;
	}

	r = archive_read_next_header(a, &ae);
	if (r != ARCHIVE_OK && r != ARCHIVE_WARN) {
		fprintf(stderr, "next_header failed: %s\n",
		    archive_error_string(a));
		return 1;
	}

	for (;;) {
		const void *buff;
		size_t size;
		int64_t offset;
		r = archive_read_data_block(a, &buff, &size, &offset);
		if (r == ARCHIVE_EOF)
			break;
		if (r != ARCHIVE_OK) {
			fprintf(stderr, "data_block failed: %s\n",
			    archive_error_string(a));
			break;
		}
	}

	archive_read_free(a);
	printf("done\n");
	return 0;
}
