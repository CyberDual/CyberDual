const buffer = new ArrayBuffer(257, { maxByteLength: 257 });
const view = new Uint8Array(buffer);
view.fill(0x7f);
buffer.resize(0);
if (buffer.byteLength !== 0) {
  throw new Error("resize failed");
}
