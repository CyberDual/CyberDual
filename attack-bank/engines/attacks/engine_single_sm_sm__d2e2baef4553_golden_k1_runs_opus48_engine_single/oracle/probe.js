// Allocating a resizable ArrayBuffer drives ResizableArrayBufferObject::initialize,
// which writes BYTE_LENGTH_SLOT while it still holds its freshly-allocated value.
let ab = new ArrayBuffer(8, { maxByteLength: 16 });
let view = new Uint8Array(ab);
view[0] = 1;
print(ab.byteLength);
