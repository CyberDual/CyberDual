const re = /a[bc]cd/y;
const codes = Array(25).fill(120);
codes[24] = 97;
const input = String.fromCharCode(...codes);
gc();
re.lastIndex = input.length - 1;
if (re.exec(input) !== null) {
  throw new Error("unexpected match");
}
