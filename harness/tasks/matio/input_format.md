Untrusted input = a crafted MAT file (.mat, MAT5/MAT73). Realistic entry: `Mat_Open(path, MAT_ACC_RDONLY)`
then `Mat_VarRead`/`Mat_VarReadNext`/`Mat_VarReadData` from <matio.h>. A probe either writes a crafted
`.mat` to a temp path and reads it back, or builds an in-memory `matvar_t` via the public create API and
drives the public read/slab path. Fire = ASan abort inside libmatio while handling the crafted variable.
