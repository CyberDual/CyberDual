// Witness for Bug 2061307 residual defect: IntrI8SelectColumnsOfB overlap check.
//
// The intrinsic validates every column index (< colsB) up front, then calls
// gemmology's SelectColumnsB, which reads the index list in groups of 8 while
// writing the output incrementally. If the output region overlaps the index
// list, output writes corrupt not-yet-read indices after they have passed
// validation, turning them into huge values and causing out-of-bounds reads of
// the input matrix.
//
// Here the index list sits just before the output, and the output starts
// exactly on the second group of indices (output > colIndexList, output inside
// the index-list region). A one-directional overlap check that only rejects an
// index list starting inside the output region does not catch this; a correct
// bidirectional range-overlap check does.
var g = newGlobal({newCompartment: true, systemPrincipal: true});
g.evaluate(`
  let memory = new WebAssembly.Memory({initial: 1, maximum: 1});
  let module = WebAssembly.mozIntGemm();
  let instance = new WebAssembly.Instance(module, {"": {"memory": memory}});
  let {int8_select_columns_of_b} = instance.exports;

  const rowsB = 64, colsB = 8, sizeColIndexList = 16;
  const input = 0;
  const colIndexList = 4000;   // 4-byte aligned; region [4000, 4064)
  const output = 4032;         // 64-byte aligned; starts on indices 8..15

  // Fill the input matrix B with 0xff so copied bytes become 0xffffffff when
  // they overwrite the not-yet-read indices.
  let u8 = new Uint8Array(memory.buffer);
  for (let i = 0; i < 2048; i++) u8[i] = 0xff;

  // All indices valid (< colsB) so the up-front validation passes.
  let idx = new Uint32Array(memory.buffer);
  let base = colIndexList >> 2;
  for (let i = 0; i < sizeColIndexList; i++) idx[base + i] = 0;

  try {
    int8_select_columns_of_b(input, rowsB, colsB, colIndexList, sizeColIndexList, output);
    print("call returned without error");
  } catch (e) {
    // On the correctly-fixed tree the overlap is rejected and a
    // WebAssembly.RuntimeError ('index out of bounds') is thrown.
    print("rejected: " + e);
  }
`);
