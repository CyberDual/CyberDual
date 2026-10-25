const args = Array(1025).fill(7);
args[1024] = 42;
function target() {
  return [arguments.length, arguments[0], arguments[1024]];
}
for (let i = 0; i < 4; ++i) {
  const result = Reflect.apply(target, null, args);
  if (result[0] !== 1025 || result[1] !== 7 || result[2] !== 42) {
    throw new Error('unexpected result');
  }
}
