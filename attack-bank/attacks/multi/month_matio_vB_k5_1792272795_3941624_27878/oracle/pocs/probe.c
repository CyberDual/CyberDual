#include <matio.h>

#include <stdio.h>
#include <stdlib.h>

int
main(void)
{
    const char *path = "/tmp/matio-rank11-probe.mat";
    size_t field_dims[11] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    size_t struct_dims[2] = {1, 1};
    int start[11] = {0};
    int stride[11] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    int edge[11] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    double data[1] = {42.0};
    double selected[1] = {0.0};
    mat_t *output;
    mat_t *input;
    matvar_t *field;
    matvar_t *fields[2];
    matvar_t *structure;
    matvar_t *selected_field;

    output = Mat_CreateVer(path, NULL, MAT_FT_MAT5);
    if ( output == NULL )
        return 1;
    field = Mat_VarCreate("field", MAT_C_DOUBLE, MAT_T_DOUBLE, 11, field_dims, data, 0);
    if ( field == NULL ) {
        Mat_Close(output);
        return 1;
    }
    fields[0] = field;
    fields[1] = NULL;
    structure = Mat_VarCreate("structure", MAT_C_STRUCT, MAT_T_STRUCT, 2, struct_dims, fields,
                          MAT_F_DONT_COPY_DATA);
    if ( structure == NULL || Mat_VarWrite(output, structure, MAT_COMPRESSION_ZLIB) != 0 ) {
        Mat_VarFree(field);
        Mat_Close(output);
        if ( structure != NULL )
            Mat_VarFree(structure);
        return 1;
    }
    Mat_VarFree(structure);
    Mat_VarFree(field);
    Mat_Close(output);

    input = Mat_Open(path, MAT_ACC_RDONLY);
    if ( input == NULL )
        return 1;
    structure = Mat_VarReadNextInfo(input);
    if ( structure == NULL ) {
        Mat_Close(input);
        return 1;
    }
    selected_field = Mat_VarGetStructFieldByName(structure, "field", 0);
    if ( selected_field == NULL ) {
        Mat_VarFree(structure);
        Mat_Close(input);
        return 1;
    }
    (void)Mat_VarReadData(input, selected_field, selected, start, stride, edge);
    Mat_VarFree(structure);
    Mat_Close(input);
    printf("%f\n", selected[0]);
    return 0;
}
