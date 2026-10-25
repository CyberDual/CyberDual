const instance = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module
  (type $s (struct
    (field (ref null any))
    ${(new Array(500).fill("(field i32)")).join(" ")}))
  (func (export "make") (result eqref)
    (struct.new_default $s))
)`))).exports;

oomTest(() => instance.make());
