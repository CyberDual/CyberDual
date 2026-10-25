const buffer = new ArrayBuffer(8, { maxByteLength: 16 });
new Uint8Array(buffer)[0] = 1;
buffer.resize(16);
new Uint8Array(buffer)[15] = 2;
if (buffer.byteLength !== 16) {
  throw new Error("unexpected buffer length");
}
