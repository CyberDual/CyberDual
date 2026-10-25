setPrefValue("wasm_baseline_debug", true);

const bytes = wasmTextToBinary(`(module
  (type $struct (struct (field i32)))
  (func (export "get") (result i32)
    (struct.get $struct 0 (ref.null $struct)))
)`);

const instance = new WebAssembly.Instance(new WebAssembly.Module(bytes));
try {
  instance.exports.get();
} catch (error) {
  if (!(error instanceof WebAssembly.RuntimeError)) {
    throw error;
  }
}
