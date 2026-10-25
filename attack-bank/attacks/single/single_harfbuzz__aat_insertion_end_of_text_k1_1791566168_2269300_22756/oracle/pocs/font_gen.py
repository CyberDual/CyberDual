import struct, sys

def u16(v): return struct.pack(">H", v & 0xFFFF)
def i16(v): return struct.pack(">h", v)
def u32(v): return struct.pack(">I", v & 0xFFFFFFFF)

NUM_GLYPHS = 2  # 0=.notdef, 1='A'

# ---------- head ----------
def head():
    b = b""
    b += u32(0x00010000)      # version
    b += u32(0x00010000)      # fontRevision
    b += u32(0)               # checkSumAdjustment
    b += u32(0x5F0F3CF5)      # magic
    b += u16(0)               # flags
    b += u16(1000)            # unitsPerEm
    b += struct.pack(">q", 0) # created
    b += struct.pack(">q", 0) # modified
    b += i16(0) + i16(0) + i16(1000) + i16(1000)  # bbox
    b += u16(0)               # macStyle
    b += u16(8)               # lowestRecPPEM
    b += i16(2)               # fontDirectionHint
    b += i16(0)               # indexToLocFormat (short)
    b += i16(0)               # glyphDataFormat
    return b

# ---------- maxp (0.5) ----------
def maxp():
    return u32(0x00005000) + u16(NUM_GLYPHS)

# ---------- hhea ----------
def hhea():
    b = b""
    b += u32(0x00010000)   # version
    b += i16(800)          # ascender
    b += i16(-200)         # descender
    b += i16(0)            # lineGap
    b += u16(1000)         # advanceWidthMax
    b += i16(0) + i16(0) + i16(1000)   # minLSB, minRSB, xMaxExtent
    b += i16(1) + i16(0) + i16(0)      # caretSlopeRise, Run, offset
    b += i16(0)*4          # reserved
    b += i16(0)            # metricDataFormat
    b += u16(NUM_GLYPHS)   # numberOfHMetrics
    return b

# ---------- hmtx ----------
def hmtx():
    b = b""
    for g in range(NUM_GLYPHS):
        b += u16(1000) + i16(0)   # advance, lsb
    return b

# ---------- cmap (format 4, platform 3 enc 1) ----------
def cmap():
    # map 'A' (0x41) -> glyph 1. Single segment covering 0x41..0x41 plus 0xFFFF terminator.
    segcount = 2
    endCode = [0x41, 0xFFFF]
    startCode = [0x41, 0xFFFF]
    # idDelta so that glyph = (char + delta) & 0xFFFF. For 0x41->1: delta = 1-0x41 = -0x40.
    idDelta = [(1 - 0x41) & 0xFFFF, 1]
    idRangeOffset = [0, 0]
    seg2 = segcount * 2
    import math
    searchRange = 2 * (2 ** (segcount.bit_length() - 1))
    entrySelector = searchRange.bit_length() - 2
    if entrySelector < 0: entrySelector = 0
    rangeShift = seg2 - searchRange
    sub = b""
    sub += u16(4)                 # format
    length_pos = len(sub)
    sub += u16(0)                 # length placeholder
    sub += u16(0)                 # language
    sub += u16(seg2)
    sub += u16(searchRange)
    sub += u16(entrySelector)
    sub += u16(rangeShift)
    for v in endCode: sub += u16(v)
    sub += u16(0)                 # reservedPad
    for v in startCode: sub += u16(v)
    for v in idDelta: sub += u16(v)
    for v in idRangeOffset: sub += u16(v)
    sub = sub[:length_pos] + u16(len(sub)) + sub[length_pos+2:]
    # cmap header: version, numTables=1, one record platform 3 enc 1
    hdr = u16(0) + u16(1)
    rec = u16(3) + u16(1) + u32(4 + 8)  # offset after header(4) + one record(8)
    return hdr + rec + sub

# ---------- morx ----------
def morx():
    nClasses = 5
    # --- InsertionSubtable body (machine start) ---
    # header: nClasses, classTable, stateArray, entryTable, insertionAction (5 x u32 = 20 bytes)
    classTable_off = 20
    # class lookup: LookupFormat0 -> format(u16)=0, array[NUM_GLYPHS] u16
    class_values = [1, 4]   # g0 -> OOB(1), g1 -> class 4
    classTable = u16(0) + b"".join(u16(c) for c in class_values)   # 2 + 2*2 = 6 bytes
    stateArray_off = classTable_off + len(classTable)   # 26
    # state array: nStates x nClasses u16 entry indices
    # state0: [A,A,A,A,B]=[0,0,0,0,1]; state1:[C,A,A,A,B]=[2,0,0,0,1]
    states = [0,0,0,0,1, 2,0,0,0,1]
    stateArray = b"".join(u16(s) for s in states)        # 10*2 = 20 bytes
    entryTable_off = stateArray_off + len(stateArray)    # 46
    # entries: newState(u16), flags(u16), currentInsertIndex(u16), markedInsertIndex(u16)
    import os
    cflags = int(os.environ.get("C_FLAGS", "0x0020"), 16)
    ccur = int(os.environ.get("C_CUR", "0x0000"), 16)
    cmark = int(os.environ.get("C_MARK", "0xFFFF"), 16)
    bflags = int(os.environ.get("B_FLAGS", "0x0000"), 16)
    entries = b""
    entries += u16(0) + u16(0x0000) + u16(0xFFFF) + u16(0xFFFF)   # A
    entries += u16(1) + u16(bflags) + u16(0xFFFF) + u16(0xFFFF)   # B
    entries += u16(0) + u16(cflags) + u16(ccur) + u16(cmark)      # C
    insertionAction_off = entryTable_off + len(entries)   # 70
    insertionAction = u16(int(os.environ.get("INSERT_GID", "1"), 16))

    body = u32(nClasses) + u32(classTable_off) + u32(stateArray_off) + u32(entryTable_off) + u32(insertionAction_off)
    body += classTable + stateArray + entries + insertionAction
    # ChainSubtable: length, coverage, subFeatureFlags
    cs_len = 12 + len(body)
    chainsub = u32(cs_len) + u32(0x00000005) + u32(0x00000001) + body
    # Chain: defaultFlags, length, featureCount, subtableCount, (features), subtable
    chain_len = 16 + len(chainsub)
    chain = u32(0x00000001) + u32(chain_len) + u32(0) + u32(1) + chainsub
    # morx header
    tbl = u16(2) + u16(0) + u32(1) + chain
    return tbl

def sfnt(tables):
    tags = sorted(tables.keys())
    numTables = len(tags)
    # header
    import math
    entrySelector = (numTables).bit_length() - 1
    searchRange = (2 ** entrySelector) * 16
    rangeShift = numTables * 16 - searchRange
    out = u32(0x00010000) + u16(numTables) + u16(searchRange) + u16(entrySelector) + u16(rangeShift)
    offset = 12 + 16 * numTables
    records = b""
    body = b""
    for tag in tags:
        data = tables[tag]
        # checksum
        padded = data + b"\x00" * ((4 - len(data) % 4) % 4)
        csum = 0
        for i in range(0, len(padded), 4):
            csum = (csum + struct.unpack(">I", padded[i:i+4])[0]) & 0xFFFFFFFF
        records += tag.encode("latin1") + u32(csum) + u32(offset) + u32(len(data))
        body += padded
        offset += len(padded)
    return out + records + body

import os as _os
tables = {
    "head": head(),
    "maxp": maxp(),
    "hhea": hhea(),
    "hmtx": hmtx(),
    "cmap": cmap(),
    "morx": morx(),
}
if _os.environ.get("TRAIL"):
    # 'post' sorts after 'morx', so the morx table is not last in the file
    tables["post"] = u32(0x00030000) + b"\x00" * 28
with open(sys.argv[1], "wb") as f:
    f.write(sfnt(tables))
print("wrote", sys.argv[1])
