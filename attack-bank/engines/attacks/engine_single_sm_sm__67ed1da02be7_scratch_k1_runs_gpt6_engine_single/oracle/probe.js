const module = new WebAssembly.Module(wasmTextToBinary(`(module
  (type $s (struct (field ${"i64 ".repeat(1024)})))
  (global (ref null $s) (struct.new_default $s))
)`));

for (let i = 1; i < 160; i++) {
  oomAtAllocation(i);
  try {
    new WebAssembly.Instance(module);
  } catch (e) {
  } finally {
    resetOOMFailure();
  }
}

print("completed");
