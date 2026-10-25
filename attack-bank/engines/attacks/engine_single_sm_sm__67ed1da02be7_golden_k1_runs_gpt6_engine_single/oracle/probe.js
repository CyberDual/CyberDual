const fields = `(field (ref null extern))`.repeat(17);
const mod = new WebAssembly.Module(wasmTextToBinary(`(module (type $s (struct ${fields})) (global (ref null $s) (struct.new_default $s)))`));
fullcompartmentchecks(true);
gc();
for (let i = 1; i < 40; i++) {
  oomAfterAllocations(i);
  let failed = false;
  try { new WebAssembly.Instance(mod); } catch (e) { failed = true; }
  resetOOMFailure();
  if (failed) print('failed', i);
  gc();
}
print('done');
