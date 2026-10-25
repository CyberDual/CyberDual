#!/usr/bin/env python3
"""Generate small crafted v5 .mat files used to exercise the MCOS
enumeration-struct path (Mat_Open + Mat_VarReadNext)."""
import struct
import sys

MI_INT8 = 1
MI_UINT8 = 2
MI_INT16 = 3
MI_UINT16 = 4
MI_INT32 = 5
MI_UINT32 = 6
MI_MATRIX = 14

MX_STRUCT = 2
MX_UINT8 = 9
MX_UINT16 = 11
MX_UINT32 = 13


def u32(x):
    return struct.pack("<I", x)


def element(dtype, data):
    """Normal-format data element, padded to an 8-byte boundary."""
    pad = (-len(data)) % 8
    return u32(dtype) + u32(len(data)) + data + b"\0" * pad


def small_element(dtype, value):
    """Small-format element (<=4 data bytes carried in the tag)."""
    return u32((len(value) << 16) | dtype) + value.ljust(4, b"\0")


def field_matrix(cls, dims, data_element):
    af = element(MI_UINT32, u32(cls) + u32(0))
    dm = element(MI_INT32, b"".join(u32(d) for d in dims))
    nm = element(MI_INT8, b"")
    return element(MI_MATRIX, af + dm + nm + data_element)


def struct_matrix(varname, fieldname, field_el, dims=(1, 1)):
    fname = fieldname.encode().ljust(24, b"\0")
    af = element(MI_UINT32, u32(MX_STRUCT) + u32(0))
    dm = element(MI_INT32, b"".join(u32(d) for d in dims))
    nm = element(MI_INT8, varname.encode())
    fnl = small_element(MI_INT32, u32(24))
    fns = element(MI_INT8, fname)
    return element(MI_MATRIX, af + dm + nm + fnl + fns + field_el)


def mat_file(data_elements):
    hdr = b"MATLAB 5.0 MAT-file, Platform: GLNXA64, Created by hand"
    hdr = hdr.ljust(116, b" ") + b" " * 8 + struct.pack("<H", 0x0100) + b"IM"
    return hdr + data_elements


def main():
    which, out = sys.argv[1], sys.argv[2]

    if which == "p0":
        # uint32 tag with a zero-length dimension product: the reader hands
        # back a zero-byte allocation that IsEnumStruct reads 4 bytes from.
        field = field_matrix(MX_UINT32, (0, 1), element(MI_UINT32, b""))
        data = struct_matrix("e", "EnumerationInstanceTag", field)
    elif which == "new":
        # tag stored as uint16: 2-byte allocation
        field = field_matrix(MX_UINT16, (1, 1), element(MI_UINT16, u32(0)[:2]))
        data = struct_matrix("e", "EnumerationInstanceTag", field)
    else:
        raise SystemExit("unknown kind: " + which)

    with open(out, "wb") as f:
        f.write(mat_file(data))
    print("wrote", out)


if __name__ == "__main__":
    main()
