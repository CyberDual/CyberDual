const binary = wasmTextToBinary(`
  (module
    (type $array (array v128))
    (func (export "make") (param i32) (result (ref $array))
      local.get 0
      array.new_default $array))
`);

const instance = new WebAssembly.Instance(new WebAssembly.Module(binary));
const array = instance.exports.make(1);
gc();
