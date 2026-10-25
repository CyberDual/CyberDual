// Residual-defect witness for the "missing debug-mode wasm OOB stackmaps" task.
//
// A Debugger observing the global forces wasm modules instantiated there to be
// compiled by the wasm baseline compiler in debug mode. In that mode every
// linear-memory instruction that can raise Trap::OutOfBounds must have an
// associated stackmap; the baseline compiler verifies this at the end of each
// function (CheckStackMapsForTraps).
//
// The fix in this tree registers OOB stackmaps for plain loads/stores and for
// atomic exchange / compare-exchange, but not for the 32-bit atomic
// read-modify-write path (i32.atomic.rmw.*). Compiling a debug-mode module that
// uses i32.atomic.rmw.add therefore trips the verifier and aborts.
//
// On the parent + real upstream fix this compiles and runs fine.

if (typeof wasmDebuggingEnabled !== "function" || !wasmDebuggingEnabled()) {
  quit(0);
}

var g = newGlobal({newCompartment: true});
var dbg = new Debugger(g);
dbg.onEnterFrame = function() {};

g.eval(`
  var wasm = wasmTextToBinary(\`(module
    (memory 1)
    (func (export "f") (param i32) (param i32) (result i32)
      local.get 0
      local.get 1
      i32.atomic.rmw.add
    )
  )\`);
  var m = new WebAssembly.Instance(new WebAssembly.Module(wasm));
  m.exports.f(0, 1);
`);
