// Witness for a stack-buffer-overflow in js::intgemm::IntrI8SelectColumnsOfB.
//
// The intrinsic copies the caller-supplied column index list into a fixed-size
// local buffer sized SELECTED_COLUMNS_B_MULTIPLIER (8). When sizeColIndexList is
// a larger (still valid) multiple of 8, the copy loop writes past the end of
// that stack buffer. All arguments below are accepted by every bounds/size
// check, so on the fixed tree ASan reports a stack-buffer-overflow write; on the
// parent tree the kernel runs normally and the call returns without error.
//
// mozIntGemm is only exposed to privileged (system principal) realms, so the
// work runs inside a system-principal global. On a Nightly build the
// javascript.options.wasm_moz_intgemm pref defaults to true.

const PROBE = `
  const memory = new WebAssembly.Memory({initial: 1, maximum: 1});
  const module = WebAssembly.mozIntGemm();
  if (!module) {
    throw new Error("mozIntGemm module unavailable");
  }
  const instance = new WebAssembly.Instance(module, {"": {"memory": memory}});
  const {int8_select_columns_of_b} = instance.exports;

  const ROWS_B = 64;   // ROWS_B_MULTIPLIER
  const COLS_B = 8;    // COLUMNS_B_MULTIPLIER
  const SIZE_COL_INDEX_LIST = 16;  // valid: multiple of SELECTED_COLUMNS_B_MULTIPLIER (8)

  const inputMatrixB = 0;      // 64-byte aligned, holds ROWS_B * COLS_B int8 = 512 bytes
  const colIndexList = 512;    // SIZE_COL_INDEX_LIST * 4 = 64 bytes
  const output = 1024;         // ROWS_B * SIZE_COL_INDEX_LIST int8 = 1024 bytes

  // Every selected column index is a valid column of B (< COLS_B), so the index
  // validation passes and the out-of-range copy is what fails.
  const indices = new Uint32Array(memory.buffer);
  for (let i = 0; i < SIZE_COL_INDEX_LIST; i++) {
    indices[(colIndexList >> 2) + i] = 0;
  }

  int8_select_columns_of_b(inputMatrixB, ROWS_B, COLS_B, colIndexList,
                           SIZE_COL_INDEX_LIST, output);
`;

const g = newGlobal({newCompartment: true, systemPrincipal: true});
g.evaluate(PROBE);
