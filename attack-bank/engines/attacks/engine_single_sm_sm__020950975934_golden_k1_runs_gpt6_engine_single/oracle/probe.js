const global = newGlobal({newCompartment: true});
const debuggerForGlobal = new Debugger(global);

global.eval(`
  var instance = new WebAssembly.Instance(
    new WebAssembly.Module(wasmTextToBinary(
      '(module (memory 1) (func (export "read") (param i32) (result i64) local.get 0 i64.load))'
    ))
  );
`);

try {
  global.instance.exports.read(65536);
} catch (error) {
  if (!String(error).includes("out of bounds")) {
    throw error;
  }
}
