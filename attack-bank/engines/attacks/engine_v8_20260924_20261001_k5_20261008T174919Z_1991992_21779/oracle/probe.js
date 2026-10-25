const chars = Array.from({length: 20}, (_, i) =>
    String.fromCharCode(0x410 + i));
const pattern = new RegExp(`(?<=${chars.join('|')})x`, 'iu');
pattern.test(`${chars[0]}x`);
