#include <matio.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static int
write_probe(const char *path)
{
    const char *fields[] = {"EnumerationInstanceTag", NULL};
    size_t parent_dims[2] = {1, 1};
    size_t tag_dims[2] = {1, 0};
    mat_uint32_t tag_data = 0;
    mat_t *mat;
    matvar_t *parent;
    matvar_t *tag;
    int result;

    mat = Mat_CreateVer(path, NULL, MAT_FT_MAT5);
    if ( mat == NULL )
        return 0;

    parent = Mat_VarCreateStruct2("probe", 2, parent_dims, fields);
    tag = Mat_VarCreate("EnumerationInstanceTag", MAT_C_UINT32, MAT_T_UINT32, 2, tag_dims,
                        &tag_data, 0);
    if ( parent == NULL || tag == NULL ) {
        Mat_VarFree(tag);
        Mat_VarFree(parent);
        Mat_Close(mat);
        return 0;
    }

    Mat_VarSetStructFieldByName(parent, fields[0], 0, tag);
    result = Mat_VarWrite(mat, parent, MAT_COMPRESSION_NONE) == 0;
    Mat_VarFree(parent);
    Mat_Close(mat);
    return result;
}

static int
adjust_probe(const char *path)
{
    static const unsigned char padding[8] = {0};
    FILE *file;
    long size;
    long offset;
    int matrices = 0;
    uint32_t data_length = 3;

    file = fopen(path, "r+b");
    if ( file == NULL )
        return 0;
    if ( fseek(file, 0, SEEK_END) != 0 ) {
        fclose(file);
        return 0;
    }
    size = ftell(file);
    if ( size < 136 ) {
        fclose(file);
        return 0;
    }

    for ( offset = 128; offset + 8 <= size; offset += 4 ) {
        uint32_t type;
        uint32_t nbytes;

        if ( fseek(file, offset, SEEK_SET) != 0 || fread(&type, sizeof(type), 1, file) != 1 ||
             fread(&nbytes, sizeof(nbytes), 1, file) != 1 || type != MAT_T_MATRIX ) {
            continue;
        }

        nbytes += 8;
        if ( fseek(file, offset + 4, SEEK_SET) != 0 ||
             fwrite(&nbytes, sizeof(nbytes), 1, file) != 1 ) {
            fclose(file);
            return 0;
        }
        matrices++;
        if ( matrices == 2 )
            break;
    }

    if ( matrices != 2 || fseek(file, -4, SEEK_END) != 0 ||
         fwrite(&data_length, sizeof(data_length), 1, file) != 1 ||
         fwrite(padding, sizeof(padding), 1, file) != 1 ) {
        fclose(file);
        return 0;
    }
    fclose(file);
    return 1;
}

int
main(void)
{
    const char *path = "residual.mat";
    mat_t *mat;
    matvar_t *var;

    if ( !write_probe(path) || !adjust_probe(path) )
        return 1;

    mat = Mat_Open(path, MAT_ACC_RDONLY);
    var = mat == NULL ? NULL : Mat_VarReadNext(mat);
    Mat_VarFree(var);
    Mat_Close(mat);
    remove(path);

    return var == NULL;
}
