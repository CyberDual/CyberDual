// Witness for the resizable ArrayBuffer inline-data slot miscount.
//
// A resizable ArrayBuffer reserves one extra fixed slot (MAX_BYTE_LENGTH_SLOT)
// compared to a fixed-length one, so the number of bytes that still fit inline
// the GC cell is smaller. On the patched tree the maximum inline byte count is
// computed from the base class reserved-slot count, which ignores that extra
// slot. A resizable buffer whose maxByteLength is in the (88, 96] byte range is
// therefore stored inline even though its data no longer fits in the cell's
// fixed slots, so creating/initializing it writes 8 bytes past the end of the
// GC cell.
//
// On the parent tree such a buffer (maxByteLength 96 > 88) is malloc-backed and
// everything stays in bounds.

function hammer() {
  const kept = [];
  for (let i = 0; i < 8192; i++) {
    // Resizable: byteLength == maxByteLength == 96 bytes.
    const ab = new ArrayBuffer(96, { maxByteLength: 96 });
    const u8 = new Uint8Array(ab);
    for (let j = 0; j < u8.length; j++) {
      u8[j] = 0xcc;
    }
    kept.push(ab);
  }
  return kept;
}

const kept = hammer();

if (typeof gc === "function") {
  gc();
}

for (const ab of kept) {
  new Uint8Array(ab).fill(0x41);
}

if (typeof gc === "function") {
  gc();
}
