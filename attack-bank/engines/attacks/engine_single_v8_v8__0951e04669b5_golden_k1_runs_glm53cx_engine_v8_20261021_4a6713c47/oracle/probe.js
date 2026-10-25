const bytes = [0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00];

function uleb(value) {
  const result = [];
  do {
    let byte = value & 0x7f;
    value >>>= 7;
    result.push(value ? byte | 0x80 : byte);
  } while (value);
  return result;
}

function section(id, contents) {
  bytes.push(id);
  bytes.push(...uleb(contents.length));
  bytes.push(...contents);
}

const type = [0x01, 0x60];
type.push(...uleb(0));
type.push(...uleb(1000));
for (let i = 0; i < 1000; ++i) type.push(0x7f);
section(1, type);
section(3, [0x01, 0x00]);
section(7, [0x01, 0x01, 0x66, 0x00, 0x00]);
const body = [0x00];
for (let i = 0; i < 1000; ++i) body.push(0x41, 0x00);
body.push(0x0b);
section(10, [0x01, ...uleb(body.length), ...body]);

const moduleBytes = new Uint8Array(bytes);
const instance = new WebAssembly.Instance(new WebAssembly.Module(moduleBytes));

function exhaust() {
  try {
    exhaust();
  } catch (error) {
    instance.exports.f();
    return;
  }
}

exhaust();
