// Witness for an ArrayBuffer byte-length corruption.
//
// A resizable ArrayBuffer is created whose current byte length is exactly
// 2 GiB (0x80000000). The backing store is allocated to maxByteLength
// (also 2 GiB). Resizing it to its current length is a no-op on a correct
// engine: resize() only clears bytes when the new length is strictly smaller
// than the old length, and here they are equal.
//
// On the vulnerable tree the stored byte length is read back as a corrupted,
// far-larger value, so resize() believes the buffer is shrinking from a huge
// old length and memset()s far past the 2 GiB allocation, triggering an
// AddressSanitizer heap-buffer-overflow (or a fatal out-of-bounds write).

const LEN = 0x80000000; // 2 GiB

var ab = new ArrayBuffer(LEN, { maxByteLength: LEN });

// No-op resize on a correct engine; out-of-bounds write on the vulnerable one.
ab.resize(LEN);

print("no crash");
