// Exercise case-insensitive single-character alternatives in a lookbehind.
const expression = /(?<=\u017F|a|b)z/i;
for (let i = 0; i < 8; i++) {
  if (!expression.test('az')) throw new Error('unexpected regexp result');
}
