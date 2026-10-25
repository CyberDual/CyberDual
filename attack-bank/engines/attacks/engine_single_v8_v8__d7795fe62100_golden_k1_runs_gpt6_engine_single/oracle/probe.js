const names = Array.from({length: 0x10000}, (_, i) => 'p' + i);
const source = 'function victim(' + names.join(',') + '){p65535=1;}';
try {
  (0, eval)(source);
  globalThis.victim();
} catch (e) {
  if (!(e instanceof SyntaxError)) throw e;
}
