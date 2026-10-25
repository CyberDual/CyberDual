const g = newGlobal({newCompartment: true});
const dbg = new Debugger(g);
g.eval(`
  const module = new WebAssembly.Module(wasmTextToBinary(` + "`" + `(module
    (type $t (func))
    (func (export "run")
      ref.null $t
      call_ref $t)
  )` + "`" + `));
  const instance = new WebAssembly.Instance(module);
  try { instance.exports.run(); } catch (e) { if (!(e instanceof WebAssembly.RuntimeError)) throw e; }
`);
