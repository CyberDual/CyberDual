// Witness for a memory-safety defect in WasmStructObject::createStructOOL.
//
// A wasm GC struct whose fields do not fit in the inline area is allocated
// out-of-line (OOL). `struct.new_default` zero-initializes such a struct. When
// the returned struct is read with `struct.get` on an OOL field, the engine
// loads the out-of-line data pointer from the object's inline area and
// dereferences it. On a correct build that pointer addresses the OOL block and
// the read returns 0. On the affected build the OOL pointer has been left as a
// null value, so the read dereferences a near-null address and crashes.

function wat2wasm(text) {
  return new WebAssembly.Module(wasmTextToBinary(text));
}

// 30 i64 fields = 240 bytes of payload, which exceeds the inline capacity, so
// the type requires out-of-line storage and the last fields live in the OOL
// block.
const fields = "(field i64)".repeat(30);

const mod = wat2wasm(`
  (module
    (type $s (struct ${fields}))
    (func (export "run") (result i64)
      (struct.get $s 29 (struct.new_default $s))))
`);

const inst = new WebAssembly.Instance(mod);

// On the parent build this returns 0. On the edited build it dereferences a
// null out-of-line pointer and crashes with SIGSEGV / an AddressSanitizer
// report.
const v = inst.exports.run();
print("result = " + v);
