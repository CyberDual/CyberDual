const realm = newGlobal({newCompartment: true, systemPrincipal: true});
realm.evaluate(`
  const memory = new WebAssembly.Memory({initial: 1, maximum: 1});
  const instance = new WebAssembly.Instance(WebAssembly.mozIntGemm(), {
    "": {memory}
  });
  const indices = new Uint32Array(memory.buffer);
  for (let i = 0; i < 8; i++) {
    indices[128 + i] = 0;
  }
  try {
    instance.exports.int8_select_columns_of_b(0, 64, 8, 512, 8, 65472);
  } catch (error) {
    print(error);
  }
`);
