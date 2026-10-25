/* Probe: read a crafted MAT v5 file containing a compressed 1x1 struct
 * whose fixed-width field name buffer holds one real name followed by an
 * empty (all-NUL) entry, through the public matio API.
 *
 * Expected: clean exit on a correct library.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <matio.h>

static const unsigned char mat_file[] = {
    77,65,84,76,65,66,32,53,46,48,32,77,65,84,45,102,105,108,101,44,32,80,108,97,
    116,102,111,114,109,58,32,71,76,78,88,65,54,52,44,32,67,114,101,97,116,101,100,32,
    98,121,32,112,114,111,98,101,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,
    32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,
    32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,0,0,0,0,
    0,0,0,0,0,1,73,77,15,0,0,0,44,0,0,0,120,218,227,99,96,96,8,0,
    98,54,32,230,0,98,38,6,8,96,133,242,25,225,152,145,161,24,44,206,2,23,23,0,
    226,10,6,84,192,7,165,1,75,246,1,155,0,0,0,0
};

int
main(void)
{
    char path[] = "/tmp/probe_struct_XXXXXX";
    mat_t *mat;
    matvar_t *var;
    FILE *fp;
    int fd;

    fd = mkstemp(path);
    if ( fd < 0 )
        return 2;
    fp = fdopen(fd, "wb");
    if ( NULL == fp )
        return 2;
    if ( fwrite(mat_file, 1, sizeof(mat_file), fp) != sizeof(mat_file) )
        return 2;
    fclose(fp);

    mat = Mat_Open(path, MAT_ACC_RDONLY);
    if ( NULL == mat )
        return 3;

    while ( NULL != (var = Mat_VarReadNext(mat)) ) {
        Mat_VarFree(var);
    }

    Mat_Close(mat);
    remove(path);
    return 0;
}
