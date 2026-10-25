const buffer = new ArrayBuffer(64, { maxByteLength: 128 });

(function () {
  const view = new Uint8Array(buffer);
  view[0] = 42;
})();

gc();
buffer.resize(96);
