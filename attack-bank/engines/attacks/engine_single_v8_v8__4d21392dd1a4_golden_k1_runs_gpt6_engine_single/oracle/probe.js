let buffer = new ArrayBuffer(128, {maxByteLength: 256});
let bytes = new Uint8Array(buffer);
bytes[0] = 37;

%CollectGarbage(0);

buffer.resize(192);
let copy = buffer.slice(0);
if (new Uint8Array(copy)[0] !== 37) throw new Error("copy failed");
