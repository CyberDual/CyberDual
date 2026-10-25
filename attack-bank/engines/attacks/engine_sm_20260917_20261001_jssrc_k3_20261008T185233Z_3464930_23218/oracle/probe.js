const input = 'x'.repeat(31) + 'ab';
const pattern = /a[bc]/my;
pattern.lastIndex = input.length - 2;
if (!pattern.test(input)) {
  throw new Error('unexpected RegExp result');
}
