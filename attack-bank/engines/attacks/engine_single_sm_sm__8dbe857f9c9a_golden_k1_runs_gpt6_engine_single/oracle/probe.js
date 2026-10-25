const g = newGlobal({newCompartment: true});
const dbg = new Debugger(g);

g.wat = `(module
  (type $array (array i32))
  (func (export "run") (result i32)
    ref.null $array
    array.len
  )
)`;

g.eval(`
  const module = new WebAssembly.Module(wasmTextToBinary(wat));
  const instance = new WebAssembly.Instance(module);
  try {
    instance.exports.run();
  } catch (e) {
    if (!(e instanceof WebAssembly.RuntimeError)) {
      throw e;
    }
  }
`);
