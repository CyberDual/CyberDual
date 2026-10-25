const memory = new WebAssembly.Memory({initial: 1, maximum: 1, shared: true});
const bytes = new Uint8Array([
  0, 97, 115, 109, 1, 0, 0, 0,
  1, 4, 1, 96, 0, 0,
  2, 11, 1, 1, 109, 3, 109, 101, 109, 2,
  3, 1, 1,
  3, 2, 1, 0,
  7, 8, 1, 4, 102, 105, 108, 108, 0, 0,
  10, 16, 1, 14, 0,
  65, 0,
  65, 193, 0,
  65, 129, 128, 4,
  252, 11, 0,
  11
]);
const instance = new WebAssembly.Instance(
    new WebAssembly.Module(bytes), {m: {mem: memory}});
try {
  instance.exports.fill();
  print('filled');
} catch (error) {
  print('caught:' + error);
}
