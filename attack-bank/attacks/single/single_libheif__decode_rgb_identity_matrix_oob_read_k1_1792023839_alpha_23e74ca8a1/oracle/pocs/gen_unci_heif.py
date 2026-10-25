#!/usr/bin/env python3
"""Generate planar uncompressed ('unci') HEIF test files from the repository
template tests/data/uncompressed_comp_RGB.heif.

The template holds a 30x20 planar 8-bit RGB image with component-interleaved
uncompressed data. This tool rewrites it into arbitrary dimensions and
per-component bit depths, extending the mdat payload accordingly:

    python3 gen_unci_heif.py <out.heif> <width> <height> <r_bits> <g_bits> <b_bits>

It only edits existing box payload fields (ispe size, uncC component depths,
iloc extent length, mdat size + payload), so the box structure stays exactly
as the template author wrote it.
"""

import sys
import struct


def boxes(data, start, end, skip_fullbox_header=False):
    """Iterate (type, box_start, box_end) over a sequence of ISO-BMFF boxes."""
    off = start
    while off + 8 <= end:
        (size,) = struct.unpack_from(">I", data, off)
        if size == 1:
            (lsize,) = struct.unpack_from(">Q", data, off + 8)
            hdr = 16
            size = lsize
        elif size == 0:
            hdr = 8
            size = end - off
        else:
            hdr = 8
        yield data[off + 4:off + 8], off, off + size
        off += size


def find_box(data, start, end, path, fullbox=False):
    """Walk nested box path, e.g. ['iprp','ipco','ispe'] starting inside a
    full box. Returns (box_start, box_end) of the last path element."""
    off_start, off_end = start, end
    for i, name in enumerate(path):
        # 'meta' (the first level here) is a full box: its children start
        # after the 4-byte version/flags. All deeper levels are plain
        # containers, so their children start after the 8-byte header.
        payload_off = off_start + (12 if (fullbox and i == 0) else 8)
        for btype, bs, be in boxes(data, payload_off, off_end):
            if btype == name.encode("ascii"):
                off_start, off_end = bs, be
                break
        else:
            raise RuntimeError("box %s not found" % name)
    return off_start, off_end


def main():
    if len(sys.argv) != 7:
        sys.stderr.write(__doc__)
        return 1

    out_path = sys.argv[1]
    width, height = int(sys.argv[2]), int(sys.argv[3])
    bits = [int(sys.argv[4]), int(sys.argv[5]), int(sys.argv[6])]
    for b in bits:
        if not (1 <= b <= 16):
            raise RuntimeError("component bit depth must be 1..16")

    tpl_path = "tests/data/uncompressed_comp_RGB.heif"
    data = bytearray(open(tpl_path, "rb").read())

    file_end = len(data)
    mdat_start = None
    mdat_end = None
    meta_start = meta_end = None
    for btype, bs, be in boxes(data, 0, file_end):
        if btype == b"meta":
            meta_start, meta_end = bs, be
        if btype == b"mdat":
            mdat_start, mdat_end = bs, be

    if meta_start is None or mdat_start is None:
        raise RuntimeError("template does not contain meta/mdat")

    # --- ispe: rewrite image width / height -------------------------------
    ispe_start, ispe_end = find_box(data, meta_start, meta_end,
                                    ["iprp", "ipco", "ispe"], fullbox=True)
    struct.pack_into(">II", data, ispe_start + 12, width, height)

    # --- uncC: rewrite component bit depths -------------------------------
    unc_start, unc_end = find_box(data, meta_start, meta_end,
                                  ["iprp", "ipco", "uncC"], fullbox=True)
    # uncC is a full box: 4B version/flags, 4B profile, 4B component_count,
    # then per component 2B index, 1B bit_depth_minus1, 1B format, 1B align.
    (comp_count,) = struct.unpack_from(">I", data, unc_start + 16)
    assert comp_count == len(bits), (comp_count, len(bits))
    comp_base = unc_start + 20
    for i, b in enumerate(bits):
        data[comp_base + i * 5 + 2] = b - 1

    # --- iloc: rewrite the item extent length -----------------------------
    iloc_start, iloc_end = find_box(data, meta_start, meta_end,
                                    ["iloc"], fullbox=True)
    # version 0: 4B version/flags, then 1B offset_size|length_size,
    # 1B base_offset_size|reserved, 2B item_count; then per item:
    # 2B item_id, 2B dref, base_offset (4B here), 2B extent_count,
    # then per extent: offset (0B here), length (4B here).
    ver = data[iloc_start + 8]
    assert ver == 0
    (item_count,) = struct.unpack_from(">H", data, iloc_start + 14)
    assert item_count == 1
    base_offset_size = (data[iloc_start + 13] >> 4) & 0xF
    assert base_offset_size == 4
    length_size = data[iloc_start + 12] & 0xF
    assert length_size == 4
    # item_id(+16), dref(+18), base_offset(+20..23), extent_count(+24),
    # then the single extent: offset (offset_size == 0), length (+26).
    (extent_count,) = struct.unpack_from(">H", data, iloc_start + 24)
    assert extent_count == 1
    extent_len_pos = iloc_start + 26
    payload_size = sum(((width * height * b) + 7) // 8 for b in bits)
    struct.pack_into(">I", data, extent_len_pos, payload_size)

    # --- mdat: resize the payload ------------------------------------------
    payload_off = mdat_start + 8
    old_payload = bytes(data[payload_off:mdat_end])

    planes = []
    for b in bits:
        nbytes = ((width * height * b) + 7) // 8
        plane = bytearray(nbytes)
        # simple sample pattern: plane index in every byte, ramp across rows
        for y in range(height):
            row_base = y * width
            for x in range(width):
                if b == 8:
                    plane[row_base + x] = (x * 2 + y * 3) & 0xFF
                else:
                    val = ((x * 5 + y * 11) * 257) & ((1 << b) - 1)
                    struct.pack_into(">H", plane, 2 * (row_base + x), val)
        planes.append(plane)
    new_payload = b"".join(planes)
    assert len(new_payload) == payload_size, (len(new_payload), payload_size)

    struct.pack_into(">I", data, mdat_start, 8 + payload_size)
    out = bytes(data[:payload_off]) + new_payload

    with open(out_path, "wb") as f:
        f.write(out)

    print("wrote %s (%d bytes): %dx%d, depths R=%d G=%d B=%d, mdat=%d"
          % (out_path, len(out), width, height, *bits, payload_size))
    print("base_offset (mdat payload) = %d" % payload_off)
    return 0


if __name__ == "__main__":
    sys.exit(main())
