const g = newGlobal({newCompartment: true});
const debuggerInstance = new Debugger(g);

g.eval(`
  const bytes = wasmTextToBinary(
    '(module (memory 1) (func (export "run") (param i32) (result i32) (i32.load offset=32768 (local.get 0))))'
  );
  const instance = new WebAssembly.Instance(new WebAssembly.Module(bytes));
  try {
    instance.exports.run(32768);
  } catch (error) {
    if (!(error instanceof WebAssembly.RuntimeError)) {
      throw error;
    }
  }
`);
