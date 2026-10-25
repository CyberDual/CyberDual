#include "archive.h"

#include <errno.h>

int main(void)
{
	struct archive *a = archive_read_new();
	if (a == NULL)
		return 1;

	archive_set_error(a, EINVAL, "%u", 1U);
	archive_read_free(a);
	return 0;
}
