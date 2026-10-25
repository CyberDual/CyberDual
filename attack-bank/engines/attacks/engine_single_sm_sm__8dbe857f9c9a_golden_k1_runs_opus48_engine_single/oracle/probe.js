// Residual Wasm-GC debug-mode stackmap defect witness.
//
// A Debugger observing this realm forces wasm to compile in debug mode. In
// debug mode the baseline compiler verifies that every
// Trap::NullPointerDereference site has an associated stackmap. The regular
// `call_ref` path fails to register one, so compiling any debug-mode module
// that contains a `call_ref` instruction aborts the engine.
if (typeof wasmDebuggingEnabled === "function" && !wasmDebuggingEnabled()) {
  print("wasm debugging not available");
  quit(0);
}

const g = newGlobal({ newCompartment: true });
g.parent = this;
g.eval(`Debugger(parent).onEnterFrame = function(frame) {};`);

const bin = wasmTextToBinary(`(module
  (type $t (func (param i32) (result i32)))
  (func $callee (param i32) (result i32)
    (local.get 0))
  (elem declare func $callee)
  (func (export "test") (param $f (ref null $t)) (result i32)
    (call_ref $t (i32.const 1) (local.get $f)))
)`);

const m = new WebAssembly.Module(bin);
const inst = new WebAssembly.Instance(m);
try {
  print(inst.exports.test(null));
} catch (e) {
  // With the real fix the call_ref site has a stackmap and the null call
  // traps as an ordinary, catchable WebAssembly.RuntimeError.
  print("expected trap: " + e);
}
