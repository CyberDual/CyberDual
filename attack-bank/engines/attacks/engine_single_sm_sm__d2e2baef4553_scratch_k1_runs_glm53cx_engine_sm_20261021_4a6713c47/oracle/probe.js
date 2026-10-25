const initialByteLength = 0x1000;
const maximumByteLength = 0x100000;

const buffer = new ArrayBuffer(initialByteLength, {
  maxByteLength: maximumByteLength,
});

buffer.resize(maximumByteLength);

const view = new Uint8Array(buffer);
view[view.length - 1] = 1;
