const global = newGlobal({newCompartment: true});
const dbg = new Debugger(global);

global.eval(`
  try {
    const bytes = wasmTextToBinary(\`
      (module
        (type $callee (func))
        (func (export "call")
          (ref.null $callee)
          (call_ref $callee)))
    \`);
    const instance = new WebAssembly.Instance(new WebAssembly.Module(bytes));
    instance.exports.call();
  } catch (error) {
    if (!(error instanceof WebAssembly.RuntimeError)) {
      throw error;
    }
  }
`);
