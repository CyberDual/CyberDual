function leb(n) {
  const out = [];
  do {
    let byte = n & 0x7f;
    n >>>= 7;
    out.push(byte | (n ? 0x80 : 0));
  } while (n);
  return out;
}
function section(id, payload) { return [id, ...leb(payload.length), ...payload]; }
function str(s) { return [s.length, ...Array.from(s, c => c.charCodeAt(0))]; }
const body = [
  0,
  0x20, 2,
  0x20, 0, 0xfd, 0, 0, 0,
  0x20, 1, 0xfd, 0, 0, 0,
  0xfd, 13, ...Array(16).fill(31),
  0xfd, 11, 0, 0,
  0x0b
];
const bytes = [
  0, 97, 115, 109, 1, 0, 0, 0,
  ...section(1, [1, 0x60, 3, 0x7f, 0x7f, 0x7f, 0]),
  ...section(3, [1, 0]),
  ...section(5, [1, 0, 1]),
  ...section(7, [2, ...str('memory'), 2, 0, ...str('run'), 0, 0]),
  ...section(10, [1, ...leb(body.length), ...body])
];
const instance = new WebAssembly.Instance(new WebAssembly.Module(new Uint8Array(bytes)));
const memory = new Uint8Array(instance.exports.memory.buffer);
memory[31] = 57;
instance.exports.run(0, 16, 32);
%WasmTierUpFunction(instance.exports.run);
instance.exports.run(0, 16, 32);
if (memory[32] !== 57 || memory[47] !== 57) throw new Error('unexpected output');
