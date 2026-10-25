setPrefValue("wasm_baseline_debug", true);

const text = `
(module
  (memory 1)
  (func (export "run") (param externref) (param i32) (result i32)
    local.get 1
    i32.load
    drop
    local.get 1
    i32.load))
`;

const instance = new WebAssembly.Instance(
  new WebAssembly.Module(wasmTextToBinary(text))
);

try {
  instance.exports.run(null, 0x10000);
} catch (error) {
  if (!(error instanceof WebAssembly.RuntimeError)) {
    throw error;
  }
}
