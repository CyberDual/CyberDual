function u32(value) {
  const bytes = [];
  do {
    let byte = value & 0x7f;
    value >>>= 7;
    if (value !== 0) byte |= 0x80;
    bytes.push(byte);
  } while (value !== 0);
  return bytes;
}

function section(id, payload) {
  return [id, ...u32(payload.length), ...payload];
}

const pages = 32769;
const type = section(1, [1, 0x60, 1, 0x7f, 1, 0x7f]);
const functions = section(3, [1, 0]);
const memory = section(5, [1, 3, ...u32(pages), ...u32(pages)]);
const exports = section(
    7, [1, 5, 0x70, 0x72, 0x6f, 0x62, 0x65, 0x00, 0x00]);
const body = [
  0x00,
  0x20, 0x00,
  0x41, 0x00,
  0x42, 0x00,
  0xfe, 0x01, 0x02, 0x00,
  0x0b,
];
const code = section(10, [1, ...u32(body.length), ...body]);
const moduleBytes = new Uint8Array([
  0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
  ...type,
  ...functions,
  ...memory,
  ...exports,
  ...code,
]);

const instance = new WebAssembly.Instance(new WebAssembly.Module(moduleBytes));
instance.exports.probe(0x80000000 | 0);
