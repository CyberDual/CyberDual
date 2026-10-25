const bytes = new Uint8Array([
  0, 97, 115, 109, 1, 0, 0, 0,
  0, 8, 4, 110, 97, 109, 101, 0, 1, 0,
]);
const module = new WebAssembly.Module(bytes);
wasmParsedNameSection(module, true);
