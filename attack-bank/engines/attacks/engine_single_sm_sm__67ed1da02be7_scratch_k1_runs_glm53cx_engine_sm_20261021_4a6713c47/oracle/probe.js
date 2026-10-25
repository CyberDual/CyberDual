const fields = Array.from({length: 10000}, () => "(field (mut i64))").join("\n");
const wat = `
(module
  (type $s (struct
${fields}))
  (func (export "get")
    (result i64)
    (struct.get $s 9999 (struct.new_default $s))))
`;

const module = new WebAssembly.Module(wasmTextToBinary(wat));
const instance = new WebAssembly.Instance(module);
instance.exports.get();
