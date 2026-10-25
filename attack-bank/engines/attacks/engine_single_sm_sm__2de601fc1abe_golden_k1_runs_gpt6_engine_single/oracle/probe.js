function run() {
  const memory = new WebAssembly.Memory({initial: 1, maximum: 1});
  const module = WebAssembly.mozIntGemm();
  const instance = new WebAssembly.Instance(module, {"": {memory}});
  const matrix = new Uint8Array(memory.buffer);
  matrix.fill(0xff, 0, 64 * 8);
  const indices = new Uint32Array(memory.buffer, 4096, 16);
  indices.fill(0);
  try {
    instance.exports.int8_select_columns_of_b(0, 64, 8, 4096, 16, 4100);
    throw new Error("overlapping buffers were accepted");
  } catch (error) {
    if (!(error instanceof WebAssembly.RuntimeError)) {
      throw error;
    }
  }
}

newGlobal({newCompartment: true, systemPrincipal: true})
  .evaluate(`(${run.toString()})()`);
