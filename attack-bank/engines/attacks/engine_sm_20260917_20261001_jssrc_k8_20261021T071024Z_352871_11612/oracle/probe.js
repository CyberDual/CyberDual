const buffer = createExternalArrayBuffer(33);
const bytes = new Uint8Array(buffer);
bytes[32] = 1;
